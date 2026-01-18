#pragma once

#include <vector>
#include <Arduino.h>
#include <algorithm>
#include <array>
#include <string.h>

#include <mbedtls/gcm.h>
#include <esp_system.h>

#include "pocket.hpp"

#define MAX_ADDRESS_DEPTH 15

/* ============================================================
   Raw packet layout (ESP-NOW safe)
   Includes dataLen to track actual encrypted bytes
   ============================================================ */
struct RawPacket
{
    uint8_t netId;
    uint8_t lengths;
    uint8_t dataLen; // actual bytes encrypted in rawData
    uint8_t iv[12];
    uint8_t tag[16];
    uint8_t rawData[250 - 1 - 1 - 1 - 12 - 16];
};

/* ============================================================
   secureCompare (constant-time compare)
   ============================================================ */
inline bool secureCompare(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

/* ============================================================
   AES-GCM helpers
   ============================================================ */
inline bool aesGcmEncrypt(uint8_t *data, size_t len, const uint8_t key[16], uint8_t iv[12], uint8_t tag[16])
{
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0)
        return false;

    esp_fill_random(iv, 12);

    int rc = mbedtls_gcm_crypt_and_tag(&ctx, MBEDTLS_GCM_ENCRYPT, len, iv, 12, nullptr, 0, data, data, 16, tag);
    mbedtls_gcm_free(&ctx);

    return rc == 0;
}

inline bool aesGcmDecrypt(uint8_t *data, size_t len, const uint8_t key[16], const uint8_t iv[12], const uint8_t tag[16])
{
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0)
        return false;

    int rc = mbedtls_gcm_auth_decrypt(&ctx, len, iv, 12, nullptr, 0, tag, 16, data, data);
    mbedtls_gcm_free(&ctx);

    return rc == 0;
}

/* ============================================================
   Calculate max payload
   Note: we store an extra 4 bytes for the seq at the start of rawData
   ============================================================ */
inline uint8_t maxPayloadSizePocket(const Address &srcAddress, const Address &destAddress)
{
    uint8_t srcLen = min((size_t)MAX_ADDRESS_DEPTH, srcAddress.size());
    uint8_t destLen = min((size_t)MAX_ADDRESS_DEPTH, destAddress.size());
    size_t used = srcLen + destLen + 4; // +4 for seq
    if (used >= sizeof(RawPacket::rawData))
        return 0;
    return sizeof(RawPacket::rawData) - used;
}

/* ============================================================
   Serialize Pocket
   - seq wird als erste 4 bytes in rawData geschrieben (big-endian)
   - der komplette rawData (inkl. seq + addresses + payload) wird
     mit AES-GCM verschlüsselt und mit tag versehen
   ============================================================ */
inline RawPacket serializePocket(const Pocket &p, uint8_t netId, const std::array<uint8_t, 16> &cryptoKey, uint32_t seq)
{
    RawPacket r{};
    r.netId = netId;

    uint8_t srcLen = min((size_t)MAX_ADDRESS_DEPTH, p.srcAddress.size());
    uint8_t destLen = min((size_t)MAX_ADDRESS_DEPTH, p.destAddress.size());
    r.lengths = (destLen << 4) | srcLen;

    size_t offset = 0;

    // write seq (4 bytes big-endian) as first 4 bytes of rawData
    r.rawData[offset++] = (uint8_t)((seq >> 24) & 0xFF);
    r.rawData[offset++] = (uint8_t)((seq >> 16) & 0xFF);
    r.rawData[offset++] = (uint8_t)((seq >> 8) & 0xFF);
    r.rawData[offset++] = (uint8_t)(seq & 0xFF);

    // addresses
    memcpy(r.rawData + offset, p.destAddress.data(), destLen);
    offset += destLen;
    memcpy(r.rawData + offset, p.srcAddress.data(), srcLen);
    offset += srcLen;

    // payload
    size_t payloadLen = min(sizeof(r.rawData) - offset, p.payload.size());
    memcpy(r.rawData + offset, p.payload.data(), payloadLen);
    offset += payloadLen;

    r.dataLen = offset; // store actual encrypted length (includes seq)

    aesGcmEncrypt(r.rawData, offset, cryptoKey.data(), r.iv, r.tag);

    return r;
}

/* ============================================================
   Deserialize Pocket
   - decrypt rawData
   - read seq (first 4 bytes big-endian)
   - read dest/src/payload
   ============================================================ */
inline Pocket deserializePocket(const RawPacket &r, uint8_t expectedNetId, const std::array<uint8_t, 16> &cryptoKey)
{
    Pocket p;

    if (r.netId != expectedNetId)
    {
        p.errored = true;
        return p;
    }

    RawPacket tmp = r;

    uint8_t destLen = tmp.lengths >> 4;
    uint8_t srcLen = tmp.lengths & 0x0F;

    if (destLen > MAX_ADDRESS_DEPTH || srcLen > MAX_ADDRESS_DEPTH)
    {
        p.errored = true;
        return p;
    }

    size_t dataLen = tmp.dataLen;
    if (dataLen < 4) // must at least contain seq
    {
        p.errored = true;
        return p;
    }

    if (!aesGcmDecrypt(tmp.rawData, dataLen, cryptoKey.data(), tmp.iv, tmp.tag))
    {
        p.errored = true;
        return p;
    }

    size_t offset = 0;

    // read seq (big-endian)
    uint32_t seq = 0;
    seq |= ((uint32_t)tmp.rawData[offset++]) << 24;
    seq |= ((uint32_t)tmp.rawData[offset++]) << 16;
    seq |= ((uint32_t)tmp.rawData[offset++]) << 8;
    seq |= ((uint32_t)tmp.rawData[offset++]);

    p.seq = seq;

    for (uint8_t i = 0; i < destLen; i++)
        p.destAddress.push_back(tmp.rawData[offset++]);
    for (uint8_t i = 0; i < srcLen; i++)
        p.srcAddress.push_back(tmp.rawData[offset++]);
    for (; offset < dataLen; offset++)
        p.payload.push_back(tmp.rawData[offset]);

    p.errored = false;
    return p;
}
