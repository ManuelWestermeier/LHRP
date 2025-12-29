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
   Includes dataLen to track actual encrypted bytes
   ============================================================ */
struct RawPacket
{
    uint8_t netId;
    uint8_t lengths;
    uint16_t dataLen; // actual bytes encrypted in rawData
    uint8_t iv[12];
    uint8_t tag[16];
    uint8_t rawData[250 - 1 - 1 - 2 - 12 - 16];
};

/* ============================================================
   Replay protection (64 × 16 bytes = 1024 bytes)
   ============================================================ */
inline uint8_t ReplayProtectionBuffer[16 * 64];

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
    return secureCompare(&ReplayProtectionBuffer[index * 16], tag, 16);
}

inline void replayStore(const uint8_t tag[16])
{
    uint8_t index = tag[0] & 0x3F;
    memcpy(&ReplayProtectionBuffer[index * 16], tag, 16);
}

/* ============================================================
   AES-GCM helpers with debug prints
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

    // Serial.print("[AES Encrypt] len=");
    // Serial.print(len);
    // Serial.print(", tag=");
    // for (int i = 0; i < 16; i++)
    // Serial.printf("%02X", tag[i]);
    // Serial.println();

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

    // Serial.print("[AES Decrypt] len=");
    // Serial.print(len);
    // Serial.print(", result=");
    // Serial.println(rc == 0 ? "OK" : "FAIL");

    return rc == 0;
}

/* ============================================================
   Calculate max payload
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
   Serialize Pocket
   ============================================================ */
inline RawPacket serializePocket(const Pocket &p, uint8_t netId, const std::array<uint8_t, 16> &key)
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

    r.dataLen = offset; // store actual encrypted length

    // Serial.print("[Serialize] destLen=");
    // Serial.print(destLen);
    // Serial.print(", srcLen=");
    // Serial.print(srcLen);
    // Serial.print(", payloadLen=");
    // Serial.println(payloadLen);

    aesGcmEncrypt(r.rawData, offset, key.data(), r.iv, r.tag);

    // Serial.print("[Serialize] NetID=");
    // Serial.print(netId);
    // Serial.print(", IV=");
    // for (int i = 0; i < 12; i++)
    //     // Serial.printf("%02X", r.iv[i]);
    //     // Serial.print(", TAG=");
    //     for (int i = 0; i < 16; i++)
    // Serial.printf("%02X", r.tag[i]);
    // Serial.println();

    return r;
}

/* ============================================================
   Deserialize Pocket
   ============================================================ */
inline Pocket deserializePocket(const RawPacket &r, uint8_t expectedNetId, const std::array<uint8_t, 16> &key)
{
    Pocket p;

    // Serial.print("[Deserialize] NetID received=");
    // Serial.println(r.netId);

    if (r.netId != expectedNetId)
    {
        // Serial.println("[Deserialize] NetID mismatch!");
        p.errored = true;
        return p;
    }

    RawPacket tmp = r;

    uint8_t destLen = tmp.lengths >> 4;
    uint8_t srcLen = tmp.lengths & 0x0F;

    if (destLen > MAX_ADDRESS_DEPTH || srcLen > MAX_ADDRESS_DEPTH)
    {
        // Serial.println("[Deserialize] Address lengths exceed MAX_ADDRESS_DEPTH");
        p.errored = true;
        return p;
    }

    size_t dataLen = tmp.dataLen;

    // Serial.print("[Deserialize] destLen=");
    // Serial.print(destLen);
    // Serial.print(", srcLen=");
    // Serial.print(srcLen);
    // Serial.print(", dataLen=");
    // Serial.println(dataLen);

    if (!aesGcmDecrypt(tmp.rawData, dataLen, key.data(), tmp.iv, tmp.tag))
    {
        // Serial.println("[Deserialize] AES decryption failed!");
        p.errored = true;
        return p;
    }

    if (replaySeen(tmp.tag))
    {
        // Serial.println("[Deserialize] Replay detected!");
        p.errored = true;
        return p;
    }
    replayStore(tmp.tag);

    size_t offset = 0;
    for (uint8_t i = 0; i < destLen; i++)
        p.destAddress.push_back(tmp.rawData[offset++]);
    for (uint8_t i = 0; i < srcLen; i++)
        p.srcAddress.push_back(tmp.rawData[offset++]);
    for (; offset < dataLen; offset++)
        p.payload.push_back(tmp.rawData[offset]);

    // Serial.print("[Deserialize] Successful. Payload=");
    // for (size_t i = 0; i < p.payload.size(); i++)
    // Serial.printf("%02X", p.payload[i]);
    // Serial.println();
    p.errored = false;

    return p;
}
