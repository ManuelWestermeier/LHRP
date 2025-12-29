#pragma once
#include <Arduino.h>
#include <algorithm>
#include <string.h>
#include "pocket.hpp"

#define MAX_ADDRESS_DEPTH 15

struct RawPacket
{
    uint8_t lengths;
    uint8_t rawData[249];
};

inline RawPacket serializePocket(const Pocket &p)
{
    RawPacket r{};

    uint8_t srcAddLen = min((size_t)MAX_ADDRESS_DEPTH, p.srcAddress.size());
    uint8_t destAddLen = min((size_t)MAX_ADDRESS_DEPTH, p.destAddress.size());
    r.lengths = destAddLen << 4 | srcAddLen;

    int offset = 1;
    for (int index = 0; index < destAddLen; index++)
    {
        r.rawData[offset++] = p.destAddress[index];
    }
    for (int index = 0; index < srcAddLen; index++)
    {
        r.rawData[offset++] = p.srcAddress[index];
    }

    size_t payloadLen = min(sizeof(r.rawData) - offset, p.payload.size());
    for (int index = 0; index < payloadLen; index++)
    {
        r.rawData[offset++] = p.payload[index];
    }

    return r;
}

inline Pocket deserializePocket(const RawPacket &r)
{
    Pocket p;

    uint8_t destLen = r.lengths >> 4;
    uint8_t srcLen = r.lengths & 0x0F;

    size_t offset = 1; // start after lengths byte

    // Copy destAddress safely
    for (size_t i = 0; i < destLen && offset < sizeof(r.rawData); i++)
    {
        p.destAddress.push_back(r.rawData[offset++]);
    }

    // Copy srcAddress safely
    for (size_t i = 0; i < srcLen && offset < sizeof(r.rawData); i++)
    {
        p.srcAddress.push_back(r.rawData[offset++]);
    }

    // Copy remaining bytes as payload
    while (offset < sizeof(r.rawData))
    {
        p.payload.push_back(r.rawData[offset++]);
    }

    return p;
}
