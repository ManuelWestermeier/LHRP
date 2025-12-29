#pragma once
#include <Arduino.h>
#include <algorithm>
#include <string.h>
#include "pocket.hpp"

#define MAX_ADDRESS_DEPTH 8
#define MAX_PAYLOAD 200

struct RawPacket
{
    uint8_t addressLen;
    uint16_t address[MAX_ADDRESS_DEPTH];

    uint8_t payloadLen;
    uint8_t payload[MAX_PAYLOAD];
};

inline RawPacket serializePocket(const Pocket &p)
{
    RawPacket r{};
    r.addressLen = min((size_t)MAX_ADDRESS_DEPTH, p.address.size());

    for (size_t i = 0; i < r.addressLen; i++)
        r.address[i] = p.address[i];

    r.payloadLen = min((size_t)MAX_PAYLOAD, p.payload.size());
    memcpy(r.payload, p.payload.data(), r.payloadLen);

    return r;
}

inline Pocket deserializePocket(const RawPacket &r)
{
    Pocket p;

    for (uint8_t i = 0; i < r.addressLen; i++)
        p.address.push_back(r.address[i]);

    for (uint8_t i = 0; i < r.payloadLen; i++)
        p.payload.push_back(r.payload[i]);

    return p;
}
