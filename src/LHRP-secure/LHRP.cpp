#include "LHRP.hpp"
#include <WiFi.h>
#include <esp_now.h>

LHRP_Node_Secure *LHRP_Node_Secure::instance = nullptr;

LHRP_Node_Secure::LHRP_Node_Secure(const array<uint8_t, 16> &key, initializer_list<LHRP_Peer> list)
{
    instance = this;
    this->key = key;

    bool first = true;
    uint8_t pin = 0;

    for (auto &p : list)
    {

        if (first)
        {
            node.you = p.address;
            first = false;
            ownMac = p.mac;
        }
        else
        {
            node.connections.push_back({.address = p.address, .pin = ++pin});
            peers.push_back(p);
        }
    }
}

bool LHRP_Node_Secure::begin()
{
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK)
    {
        return false;
    }

    esp_now_register_recv_cb(onReceiveStatic);

    bool allPeersAdded = true;

    for (auto &p : peers)
        if (!addPeer(p.mac))
            allPeersAdded = false;

    return allPeersAdded;
}

bool LHRP_Node_Secure::addPeer(const array<uint8_t, 6> &mac)
{
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac.data(), 6);
    peer.channel = 0;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

bool LHRP_Node_Secure::send(const Pocket &p)
{
    // The routing logic (node.send) will determine the next hop pin.
    uint8_t pin = node.send(p);

    // If the packet is for this node, handle it locally
    if (pin == 0)
    {
        if (rxCallback)
            rxCallback(p);
        return true;
    }

    RawPacket raw = serializePocket(p);

    esp_err_t err = esp_now_send(
        peers[pin - 1].mac.data(),
        (uint8_t *)&raw,
        sizeof(RawPacket));

    return err == ESP_OK;
}

void LHRP_Node_Secure::onReceiveStatic(
    const uint8_t *mac,
    const uint8_t *data,
    int len)
{
    if (instance)
        instance->onReceive(mac, data, len);
}

void LHRP_Node_Secure::onReceive(
    const uint8_t *mac,
    const uint8_t *data,
    int len)
{
    if (len != sizeof(RawPacket))
        return;

    RawPacket raw;
    memcpy(&raw, data, sizeof(RawPacket));

    Pocket p = deserializePocket(raw);

    send(p);
}