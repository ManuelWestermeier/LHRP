#pragma once
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
   ============================================================ */

struct RawPacket
{
    uint8_t netId;
    uint8_t lengths;
    uint8_t iv[12];  // AES-GCM standard
    uint8_t tag[16]; // AES-GCM auth tag
    uint8_t rawData[250 - 1 - 1 - 12 - 16];
};

/* ============================================================
   Replay protection
   64 slots × 16 bytes = 1024 bytes
  x first 8 bytes check of reuse, last 8 bytes can be a timestamp (to check if the pocket is long enough not sent again)
   index = tag[0] & 0x3F
   ============================================================ */

inline uint8_t ReplayProtectionBuffer[16 * 64];

/* ------------------------------------------------------------ */

inline bool secureCompare(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++)
        diff |= a[i] ^ b[i];
    return diff == 0;
}

inline bool replaySeen(const uint8_t tag[16])
{
    uint8_t index = tag[0] & 0x3F;
    return secureCompare(
        &ReplayProtectionBuffer[index * 16],
        tag,
        16);
}

inline void replayStore(const uint8_t tag[16])
{
    uint8_t index = tag[0] & 0x3F;
    memcpy(&ReplayProtectionBuffer[index * 16], tag, 16);
}

/* ============================================================
   AES-GCM helpers
   ============================================================ */

inline bool aesGcmEncrypt(
    uint8_t *data,
    size_t len,
    const uint8_t key[16],
    uint8_t iv[12],
    uint8_t tag[16])
{
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0)
        return false;

    esp_fill_random(iv, 12);

    int rc = mbedtls_gcm_crypt_and_tag(
        &ctx,
        MBEDTLS_GCM_ENCRYPT,
        len,
        iv, 12,
        nullptr, 0, // no AAD
        data, data,
        16, tag);

    mbedtls_gcm_free(&ctx);
    return rc == 0;
}

inline bool aesGcmDecrypt(
    uint8_t *data,
    size_t len,
    const uint8_t key[16],
    const uint8_t iv[12],
    const uint8_t tag[16])
{
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0)
        return false;

    int rc = mbedtls_gcm_auth_decrypt(
        &ctx,
        len,
        iv, 12,
        nullptr, 0,
        tag, 16,
        data, data);

    mbedtls_gcm_free(&ctx);
    return rc == 0;
}

/* ============================================================
   Max payload calculation
   ============================================================ */

inline uint8_t maxPayloadSize(const Pocket &p)
{
    uint8_t srcLen = min((size_t)MAX_ADDRESS_DEPTH, p.srcAddress.size());
    uint8_t destLen = min((size_t)MAX_ADDRESS_DEPTH, p.destAddress.size());

    size_t used = srcLen + destLen;
    if (used >= sizeof(RawPacket::rawData))
        return 0;

    return sizeof(RawPacket::rawData) - used;
}

/* ============================================================
   Serialize
   ============================================================ */

inline RawPacket serializePocket(
    const Pocket &p,
    uint8_t netId,
    const std::array<uint8_t, 16> &key)
{
    RawPacket r{};
    r.netId = netId;

    uint8_t srcLen = min((size_t)MAX_ADDRESS_DEPTH, p.srcAddress.size());
    uint8_t destLen = min((size_t)MAX_ADDRESS_DEPTH, p.destAddress.size());
    r.lengths = (destLen << 4) | srcLen;

    size_t offset = 0;

    memcpy(r.rawData + offset, p.destAddress.data(), destLen);
    offset += destLen;

    memcpy(r.rawData + offset, p.srcAddress.data(), srcLen);
    offset += srcLen;

    size_t payloadLen = min(sizeof(r.rawData) - offset, p.payload.size());
    memcpy(r.rawData + offset, p.payload.data(), payloadLen);
    offset += payloadLen;

    aesGcmEncrypt(
        r.rawData,
        offset,
        key.data(),
        r.iv,
        r.tag);

    return r;
}

/* ============================================================
   Deserialize
   ============================================================ */

inline Pocket deserializePocket(
    const RawPacket &r,
    uint8_t expectedNetId,
    const std::array<uint8_t, 16> &key)
{
    Pocket p;

    if (r.netId != expectedNetId)
    {
        p.errored = true;
        return p;
    }

    if (replaySeen(r.tag))
    {
        p.errored = true;
        return p;
    }

    RawPacket tmp = r;

    if (!aesGcmDecrypt(
            tmp.rawData,
            sizeof(tmp.rawData),
            key.data(),
            tmp.iv,
            tmp.tag))
    {
        p.errored = true;
        return p;
    }

    replayStore(r.tag);

    uint8_t destLen = tmp.lengths >> 4;
    uint8_t srcLen = tmp.lengths & 0x0F;

    if (destLen > MAX_ADDRESS_DEPTH || srcLen > MAX_ADDRESS_DEPTH)
    {
        p.errored = true;
        return p;
    }

    size_t offset = 0;

    for (uint8_t i = 0; i < destLen; i++)
        p.destAddress.push_back(tmp.rawData[offset++]);

    for (uint8_t i = 0; i < srcLen; i++)
        p.srcAddress.push_back(tmp.rawData[offset++]);

    while (offset < sizeof(tmp.rawData))
        p.payload.push_back(tmp.rawData[offset++]);

    return p;
}
