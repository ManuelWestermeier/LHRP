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
#define RAWPACKET_SIZE 250

/* ============================================================
   Raw packet layout (ESP-NOW safe, PACKED)
   ============================================================ */
struct __attribute__((packed)) RawPacket
{
    uint8_t netId;                                         // 1
    uint8_t lengths;                                       // 1  (destLen << 4 | srcLen)
    uint8_t dataLen;                                       // 1  (authenticated!)
    uint8_t iv[12];                                        // 12
    uint8_t tag[16];                                       // 16
    uint8_t rawData[RAWPACKET_SIZE - 1 - 1 - 1 - 12 - 16]; // 219
};

static_assert(sizeof(RawPacket) == RAWPACKET_SIZE, "RawPacket size mismatch");

/* ============================================================
   AES-GCM helpers (WITH AAD)
   ============================================================ */
inline bool aesGcmEncrypt(
    uint8_t *data,
    size_t len,
    const uint8_t key[16],
    uint8_t iv[12],
    uint8_t tag[16],
    const uint8_t *aad,
    size_t aadLen)
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
        aad, aadLen,
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
    const uint8_t tag[16],
    const uint8_t *aad,
    size_t aadLen)
{
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);

    if (mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 128) != 0)
        return false;

    int rc = mbedtls_gcm_auth_decrypt(
        &ctx,
        len,
        iv, 12,
        aad, aadLen,
        tag, 16,
        data, data);

    mbedtls_gcm_free(&ctx);
    return rc == 0;
}

/* ============================================================
   Max payload calculation
   ============================================================ */
inline uint8_t maxPayloadSizePocket(const Address &src, const Address &dst)
{
    uint8_t srcLen = min((size_t)MAX_ADDRESS_DEPTH, src.size());
    uint8_t dstLen = min((size_t)MAX_ADDRESS_DEPTH, dst.size());

    size_t used = 4 + srcLen + dstLen; // seq + addresses
    if (used >= sizeof(RawPacket::rawData))
        return 0;

    return sizeof(RawPacket::rawData) - used;
}

/* ============================================================
   Serialize Pocket (SAFE)
   ============================================================ */
inline RawPacket serializePocket(
    const Pocket &p,
    uint8_t netId,
    const std::array<uint8_t, 16> &key,
    uint32_t seq)
{
    RawPacket r{};
    r.netId = netId;

    uint8_t srcLen = min((size_t)MAX_ADDRESS_DEPTH, p.srcAddress.size());
    uint8_t dstLen = min((size_t)MAX_ADDRESS_DEPTH, p.destAddress.size());
    r.lengths = (dstLen << 4) | srcLen;

    size_t offset = 0;

    // seq (big-endian)
    r.rawData[offset++] = (seq >> 24) & 0xFF;
    r.rawData[offset++] = (seq >> 16) & 0xFF;
    r.rawData[offset++] = (seq >> 8) & 0xFF;
    r.rawData[offset++] = seq & 0xFF;

    memcpy(r.rawData + offset, p.destAddress.data(), dstLen);
    offset += dstLen;

    memcpy(r.rawData + offset, p.srcAddress.data(), srcLen);
    offset += srcLen;

    size_t maxPayload = sizeof(r.rawData) - offset;
    size_t payloadLen = min(maxPayload, p.payload.size());
    memcpy(r.rawData + offset, p.payload.data(), payloadLen);
    offset += payloadLen;

    r.dataLen = offset;

    uint8_t aad[3] = {r.netId, r.lengths, r.dataLen};

    aesGcmEncrypt(
        r.rawData,
        r.dataLen,
        key.data(),
        r.iv,
        r.tag,
        aad,
        sizeof(aad));

    return r;
}

/* ============================================================
   Deserialize Pocket (SAFE)
   ============================================================ */
inline Pocket deserializePocket(
    const RawPacket &r,
    uint8_t expectedNetId,
    const std::array<uint8_t, 16> &key)
{
    Pocket p{};
    p.errored = true;

    if (r.netId != expectedNetId)
        return p;

    if (r.dataLen < 4 || r.dataLen > sizeof(r.rawData))
        return p;

    uint8_t dstLen = r.lengths >> 4;
    uint8_t srcLen = r.lengths & 0x0F;

    if (dstLen > MAX_ADDRESS_DEPTH || srcLen > MAX_ADDRESS_DEPTH)
        return p;

    if (4 + dstLen + srcLen > r.dataLen)
        return p;

    RawPacket tmp = r;

    uint8_t aad[3] = {tmp.netId, tmp.lengths, tmp.dataLen};

    if (!aesGcmDecrypt(
            tmp.rawData,
            tmp.dataLen,
            key.data(),
            tmp.iv,
            tmp.tag,
            aad,
            sizeof(aad)))
        return p;

    size_t offset = 0;

    p.seq =
        (uint32_t(tmp.rawData[offset++]) << 24) |
        (uint32_t(tmp.rawData[offset++]) << 16) |
        (uint32_t(tmp.rawData[offset++]) << 8) |
        uint32_t(tmp.rawData[offset++]);

    for (uint8_t i = 0; i < dstLen; i++)
        p.destAddress.push_back(tmp.rawData[offset++]);

    for (uint8_t i = 0; i < srcLen; i++)
        p.srcAddress.push_back(tmp.rawData[offset++]);

    while (offset < tmp.dataLen)
        p.payload.push_back(tmp.rawData[offset++]);

    p.errored = false;
    return p;
}
