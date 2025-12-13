#pragma once
#include <vector>
#include <array>
#include <initializer_list>

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
    LHRP_Node(initializer_list<LHRP_Peer> peers);

    void begin();
    void send(const Pocket &p);

private:
    static LHRP_Node *instance;

    Node node;
    vector<LHRP_Peer> peers;

    static void onReceiveStatic(
        const uint8_t *mac,
        const uint8_t *data,
        int len);

    void onReceive(
        const uint8_t *mac,
        const uint8_t *data,
        int len);

    void addPeer(const array<uint8_t, 6> &mac);
};
