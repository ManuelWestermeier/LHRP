#include "LHRP.hpp"

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Preferences.h>

LHRP_Node_Secure *LHRP_Node_Secure::instance = nullptr;
unordered_map<string, LHRP_Node_Secure::PeerState> LHRP_Node_Secure::peerStates;

struct LHRP_Node_Secure::PeerState
{
    uint32_t lastSeenSeq = 0;
    uint32_t lastSendSeq = 0;
    uint32_t lastFlushTime = 0;
};

// ------------------------
inline uint8_t netIdToChannel(uint8_t netId)
{
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
    return key;
}

// ------------------------
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
            ownMac = p.mac;
            first = false;
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
        return false;

    esp_now_register_recv_cb(onReceiveStatic);

    prefs.begin("lhrp", false);

    for (auto &p : peers)
    {
        string macKey = uint8ArrayToHex(p.mac.data(), 6);
        peerStates[macKey].lastSeenSeq = prefs.getUInt(("r_" + macKey).c_str(), 0);
        peerStates[macKey].lastSendSeq = prefs.getUInt(("s_" + macKey).c_str(), 0);
        peerStates[macKey].lastFlushTime = millis();
    }

    bool allPeersAdded = true;
    for (auto &p : peers)
    {
        if (!addPeer(p.mac))
            allPeersAdded = false;
    }

    return allPeersAdded;
}

bool LHRP_Node_Secure::addPeer(const array<uint8_t, 6> &mac)
{
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, mac.data(), 6);
    peer.channel = netIdToChannel(netId);
    peer.encrypt = false;

    return esp_now_add_peer(&peer) == ESP_OK;
}

// ------------------------
uint32_t LHRP_Node_Secure::getNextSendSeq(const array<uint8_t, 6> &mac)
{
    string macKey = uint8ArrayToHex(mac.data(), 6);
    auto &state = peerStates[macKey];
    state.lastSendSeq++;
    return state.lastSendSeq;
}

void LHRP_Node_Secure::maybeFlushToNVS(const string &macKey, PeerState &state)
{
    uint32_t now = millis();
    if (now - state.lastFlushTime < 10000)
        return; // nur alle 10 Sekunden

    prefs.putUInt(("r_" + macKey).c_str(), state.lastSeenSeq);
    prefs.putUInt(("s_" + macKey).c_str(), state.lastSendSeq);
    state.lastFlushTime = now;
}

// ------------------------
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
        return false;

    if (pin == 0)
    {
        if (rxCallback)
            rxCallback(p);
        return true;
    }

    if (pin - 1 >= peers.size())
        return false;

    const array<uint8_t, 6> &peerMac = peers[pin - 1].mac;
    uint32_t seq = getNextSendSeq(peerMac);
    RawPacket raw = serializePocket(p, netId, this->key, seq);
    esp_err_t err = esp_now_send(peerMac.data(), (uint8_t *)&raw, sizeof(RawPacket));

    string macKey = uint8ArrayToHex(peerMac.data(), 6);
    maybeFlushToNVS(macKey, peerStates[macKey]);

    return err == ESP_OK;
}

// ------------------------
void LHRP_Node_Secure::onReceiveStatic(const uint8_t *mac, const uint8_t *data, int len)
{
    if (instance)
        instance->onReceive(mac, data, len);
}

void LHRP_Node_Secure::onReceive(const uint8_t *mac, const uint8_t *data, int len)
{
    if (len != sizeof(RawPacket))
        return;

    RawPacket raw;
    memcpy(&raw, data, sizeof(RawPacket));
    Pocket p = deserializePocket(raw, netId, key);
    if (p.errored)
        return;

    string macKey = uint8ArrayToHex(mac, 6);
    auto &state = peerStates[macKey];

    if ((int32_t)(p.seq - state.lastSeenSeq) <= 0)
        return;

    state.lastSeenSeq = p.seq;
    maybeFlushToNVS(macKey, state);

    send(p);
}
