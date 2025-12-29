#pragma once

#include <vector>
#include <array>
#include <initializer_list>
#include <functional>

#include <WiFi.h>
#include <esp_now.h>

#include "protocol.hpp"
#include "raw-packet.hpp"

using namespace std;

struct LHRP_Peer
{
    array<uint8_t, 6> mac;
    Address address;
};

class LHRP_Node_Secure
{
public:
    Node node;
    array<uint8_t, 6> ownMac;
    array<uint8_t, 16> key;
    uint8_t netId;

    LHRP_Node_Secure(uint8_t netId, const array<uint8_t, 16> &key, std::initializer_list<LHRP_Peer> peers);

    bool begin();
    bool send(const Pocket &p);
    bool send(const Address &dest, const vector<uint8_t> &payload);

    void onPocketReceive(std::function<void(const Pocket &)> cb)
    {
        rxCallback = cb;
    }

    // Needed for ESP-NOW static callback
    static void onReceiveStatic(const uint8_t *mac, const uint8_t *data, int len);

private:
    vector<LHRP_Peer> peers;           // Store all peers
    static LHRP_Node_Secure *instance; // Singleton for static callback
    void onReceive(const uint8_t *mac, const uint8_t *data, int len);

    std::function<void(const Pocket &)> rxCallback;

    bool addPeer(const array<uint8_t, 6> &mac);
};
