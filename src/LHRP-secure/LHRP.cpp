#include "LHRP.hpp"

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>

LHRP_Node_Secure *LHRP_Node_Secure::instance = nullptr;

inline uint8_t netIdToChannel(uint8_t netId)
{
    // Map netId → WiFi channel 1..13
    return (netId * 7 % 13) + 1;
}

static string uint8ArrayToHex(const uint8_t *arr, size_t len)
{
    static const char hexDigits[] = "0123456789ABCDEF";
    string s;
    s.reserve(len * 2);
    for (size_t i = 0; i < len; i++)
    {
        uint8_t v = arr[i];
        s.push_back(hexDigits[(v >> 4) & 0xF]);
        s.push_back(hexDigits[v & 0xF]);
    }
    return s;
}

string LHRP_Node_Secure::macToHexKey(const uint8_t *mac, const char *prefix)
{
    string machex = uint8ArrayToHex(mac, 6);
    string key = prefix;
    key += machex;
    // Preferences keys max length is sufficiently large; ensure null-termination when using c_str()
    return key;
}

LHRP_Node_Secure::LHRP_Node_Secure(uint8_t netId, const array<uint8_t, 16> &key, initializer_list<LHRP_Peer> list)
{
    instance = this;
    this->netId = netId;
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
    esp_wifi_set_channel(netIdToChannel(netId), WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK)
    {
        return false;
    }

    esp_now_register_recv_cb(onReceiveStatic);

    // open NVS namespace "lhrp"
    prefs.begin("lhrp", false);

    bool allPeersAdded = true;

    for (auto &p : peers)
    {
        if (!addPeer(p.mac))
        {
            allPeersAdded = false;
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
    if (pin == LHRP_PIN_ERROR)
    {
        return false;
    }

    if (pin == 0)
    {
        if (rxCallback)
            rxCallback(p);
        return true;
    }

    // Determine peer MAC for this pin
    if (pin - 1 >= peers.size())
        return false;
    const array<uint8_t, 6> &peerMac = peers[pin - 1].mac;

    // build prefs key and read last send seq
    string sKey = macToHexKey(peerMac.data(), "s_");
    uint32_t last = prefs.getUInt(sKey.c_str(), 0);
    uint32_t seq = last + 1; // simple monotonic increment; wraps naturally at 2^32

    // persist new send seq
    prefs.putUInt(sKey.c_str(), seq);

    // serialize with seq
    RawPacket raw = serializePocket(p, netId, this->key, seq);

    esp_err_t err = esp_now_send(peerMac.data(), (uint8_t *)&raw, sizeof(RawPacket));
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
        return;
    }

    RawPacket raw;
    memcpy(&raw, data, sizeof(RawPacket));

    Pocket p = deserializePocket(raw, netId, key);
    if (p.errored)
    {
        return;
    }

    // Replay check: use sender MAC (esp-now mac) to index persisted last seen sequence
    string rKey = macToHexKey(mac, "r_");
    uint32_t lastSeen = prefs.getUInt(rKey.c_str(), 0);

    // If received sequence is <= last seen, it's a replay
    if (p.seq <= lastSeen)
    {
        // drop packet
        return;
    }

    // Update persisted last seen seq
    prefs.putUInt(rKey.c_str(), p.seq);

    // Route packet (may be for this node or forwarded)
    send(p);
}
