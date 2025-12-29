#pragma once

#include <Arduino.h>
#include <initializer_list>

#include "LHRP-secure/LHRP.hpp"

struct NetworkConfiguration
{
    Address node1;
    Address node2;
    Address node3;
};

NetworkConfiguration networkConfiguration1 = {{1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1, 1}};

LHRP_Node_Secure getNodeSecure(uint8_t netId, const array<uint8_t, 16> &key, NetworkConfiguration networkConfiguration)
{
    LHRP_Peer peer1 = {{0x88, 0x13, 0xBF, 0x0B, 0xA6, 0x6C}, networkConfiguration.node1};
    LHRP_Peer peer2 = {{0x88, 0x13, 0xBF, 0x0B, 0x62, 0x18}, networkConfiguration.node2};
    LHRP_Peer peer3 = {{0xEC, 0xE3, 0x34, 0x9A, 0xAE, 0x74}, networkConfiguration.node3};

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    if (peer1.mac[0] == mac[0] &&
        peer1.mac[1] == mac[1] &&
        peer1.mac[2] == mac[2] &&
        peer1.mac[3] == mac[3] &&
        peer1.mac[4] == mac[4] &&
        peer1.mac[5] == mac[5])
    {
        return LHRP_Node_Secure(netId, key, {peer1, peer2});
    }
    else if (peer2.mac[0] == mac[0] &&
             peer2.mac[1] == mac[1] &&
             peer2.mac[2] == mac[2] &&
             peer2.mac[3] == mac[3] &&
             peer2.mac[4] == mac[4] &&
             peer2.mac[5] == mac[5])
    {
        return LHRP_Node_Secure(netId, key, {peer2, peer1, peer3});
    }
    else if (peer3.mac[0] == mac[0] &&
             peer3.mac[1] == mac[1] &&
             peer3.mac[2] == mac[2] &&
             peer3.mac[3] == mac[3] &&
             peer3.mac[4] == mac[4] &&
             peer3.mac[5] == mac[5])
    {
        return LHRP_Node_Secure(netId, key, {peer3, peer2});
    }

    return LHRP_Node_Secure(netId, key, {peer1, peer2, peer3});
}

bool isSender()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    return (0x88 == mac[0] &&
            0x13 == mac[1] &&
            0xBF == mac[2] &&
            0x0B == mac[3] &&
            0xA6 == mac[4] &&
            0x6C == mac[5]);
}