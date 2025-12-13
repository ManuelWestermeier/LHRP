#pragma once

#include <vector>
#include <array>
#include <initializer_list>
#include <functional>

#include <WiFi.h>
#include <esp_now.h>

#include "protocol.hpp"
#include "raw_packet.hpp"

using namespace std;

struct LHRP_Peer
{
    array<uint8_t, 6> mac;
    Address address;
};

class LHRP_Node
{
public:
    Node node;

    LHRP_Node(std::initializer_list<LHRP_Peer> peers);

    void begin();
    bool send(const Pocket &p);

    void onPocketReceive(std::function<void(const Pocket &)> cb)
    {
        rxCallback = cb;
    }

    // Needed for ESP-NOW static callback
    static void onReceiveStatic(const uint8_t *mac, const uint8_t *data, int len);

private:
    static LHRP_Node *instance; // Singleton for static callback
    void onReceive(const uint8_t *mac, const uint8_t *data, int len);

    vector<LHRP_Peer> peers; // Store all peers
    std::function<void(const Pocket &)> rxCallback;

    void addPeer(const array<uint8_t, 6> &mac);
};
