#include "LHRP.hpp"

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

LHRP_Node_Secure *LHRP_Node_Secure::instance = nullptr;

inline uint8_t netIdToChannel(uint8_t netId)
{
    // Map netId → WiFi channel 1..13
    return (netId * 7 % 13) + 1;
}

LHRP_Node_Secure::LHRP_Node_Secure(uint8_t netId, const array<uint8_t, 16> &key, initializer_list<LHRP_Peer> list)
{
    instance = this;
    this->netId = netId;
    this->key = key;

    // Serial.println("[LHRP] Initializing node...");

    bool first = true;
    uint8_t pin = 0;

    for (auto &p : list)
    {
        if (first)
        {
            node.you = p.address;
            first = false;
            ownMac = p.mac;
            // Serial.printf("[LHRP] Set self address: %u, MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            //   p.address,
            //   p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5]);
        }
        else
        {
            node.connections.push_back({.address = p.address, .pin = ++pin});
            peers.push_back(p);
            // Serial.printf("[LHRP] Added peer %u, MAC: %02X:%02X:%02X:%02X:%02X:%02X, pin: %u\n",
            //   p.address,
            //   p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5],
            //   pin);
        }
    }
}

bool LHRP_Node_Secure::begin()
{
    // Serial.println("[LHRP] Starting WiFi in STA mode...");
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(netIdToChannel(netId), WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        // Serial.println("[LHRP] ERROR: ESP-NOW init failed!");
        return false;
    }

    // Serial.println("[LHRP] ESP-NOW initialized successfully.");
    esp_now_register_recv_cb(onReceiveStatic);

    bool allPeersAdded = true;

    for (auto &p : peers)
    {
        if (!addPeer(p.mac))
        {
            // Serial.printf("[LHRP] ERROR: Failed to add peer MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
            //   p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5]);
            allPeersAdded = false;
        }
        else
        {
            // Serial.printf("[LHRP] Peer added successfully: %02X:%02X:%02X:%02X:%02X:%02X\n",
            //   p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5]);
        }
    }

    return allPeersAdded;
}

bool LHRP_Node_Secure::addPeer(const array<uint8_t, 6> &mac)
{
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac.data(), 6);
    peer.channel = netIdToChannel(netId);
    peer.encrypt = false;

    esp_err_t res = esp_now_add_peer(&peer);
    // Serial.printf("[LHRP] Adding peer %02X:%02X:%02X:%02X:%02X:%02X... %s\n",
    //   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
    //   res == ESP_OK ? "SUCCESS" : "FAILURE");
    return res == ESP_OK;
}

bool LHRP_Node_Secure::send(const Address &dest, const vector<uint8_t> &payload)
{
    Pocket p{.destAddress = dest, .srcAddress = node.you, .payload = payload};
    return send(p);
}

int LHRP_Node_Secure::maxPayloadSize(const Address &destAddress)
{
    return maxPayloadSizePocket(node.you, destAddress);
}

bool LHRP_Node_Secure::send(const Pocket &p)
{
    uint8_t pin = node.send(p);
    // Serial.printf("[LHRP] Routing pocket to pin %u\n", pin);

    if (pin == LHRP_PIN_ERROR)
    {
        // Serial.println("[LHRP] ERROR: Invalid routing pin.");
        return false;
    }

    if (pin == 0)
    {
        // Serial.println("[LHRP] Packet is for this node, calling rxCallback.");
        if (rxCallback)
            rxCallback(p);
        return true;
    }

    RawPacket raw = serializePocket(p, netId, key);
    esp_err_t err = esp_now_send(peers[pin - 1].mac.data(), (uint8_t *)&raw, sizeof(RawPacket));

    // Serial.printf("[LHRP] Sending packet to peer %u (%02X:%02X:%02X:%02X:%02X:%02X)... %s\n",
    //   pin,
    //   peers[pin - 1].mac[0], peers[pin - 1].mac[1], peers[pin - 1].mac[2],
    //   peers[pin - 1].mac[3], peers[pin - 1].mac[4], peers[pin - 1].mac[5],
    //   err == ESP_OK ? "SUCCESS" : "FAILURE");

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
    {
        // Serial.println("[LHRP] ERROR: Packet size mismatch.");
        return;
    }

    RawPacket raw;
    memcpy(&raw, data, sizeof(RawPacket));

    Pocket p = deserializePocket(raw, netId, key);
    if (p.errored)
    {
        // Serial.println("[LHRP] ERROR: Packet decryption/deserialization failed.");
        return;
    }

    // Serial.println("[LHRP] Packet deserialized successfully, routing...");
    send(p);
}
