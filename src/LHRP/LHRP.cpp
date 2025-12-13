#include "LHRP.hpp"

LHRP_Node *LHRP_Node::instance = nullptr;

LHRP_Node::LHRP_Node(initializer_list<LHRP_Peer> list)
{
    instance = this;

    bool first = true;
    uint8_t pin = 1;

    for (auto &p : list)
    {
        peers.push_back(p);

        if (first)
        {
            node.you = p.address;
            first = false;
        }
        else
        {
            node.connections.push_back({.address = p.address,
                                        .pin = pin++});
        }
    }
}

void LHRP_Node::begin()
{
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK)
    {
        Serial.println("[LHRP] ESP-NOW init failed");
        return;
    }

    esp_now_register_recv_cb(onReceiveStatic);

    for (auto &p : peers)
        addPeer(p.mac);

    Serial.println("[LHRP] ready");
}

void LHRP_Node::addPeer(const array<uint8_t, 6> &mac)
{
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac.data(), 6);
    peer.channel = 0;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
}

void LHRP_Node::send(const Pocket &p)
{
    uint8_t pin = node.send(p);
    if (pin == 0)
        return;

    RawPacket raw = serializePocket(p);

    esp_now_send(
        peers[pin].mac.data(),
        (uint8_t *)&raw,
        sizeof(RawPacket));
}

void LHRP_Node::onReceiveStatic(
    const uint8_t *mac,
    const uint8_t *data,
    int len)
{
    if (instance)
        instance->onReceive(mac, data, len);
}

void LHRP_Node::onReceive(
    const uint8_t *mac,
    const uint8_t *data,
    int len)
{
    if (len != sizeof(RawPacket))
        return;

    RawPacket raw;
    memcpy(&raw, data, sizeof(RawPacket));

    Pocket p = deserializePocket(raw);

    uint8_t out = node.recieve(p);
    if (out == 0)
        return;

    esp_now_send(
        peers[out].mac.data(),
        (uint8_t *)&raw,
        sizeof(RawPacket));
}
