#pragma once

#include <vector>
#include <array>
#include <initializer_list>
#include <unordered_map>
#include <functional>
#include <string>

#include <WiFi.h>
#include <esp_now.h>
#include <Preferences.h>

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
    int maxPayloadSize(const Address &destAddress);

    void onPocketReceive(std::function<void(const Pocket &)> cb)
    {
        rxCallback = cb;
    }

    // Needed for ESP-NOW static callback
    static void onReceiveStatic(const uint8_t *mac, const uint8_t *data, int len);

private:
    struct PeerState;
    static unordered_map<string, PeerState> peerStates;

    vector<LHRP_Peer> peers;
    static LHRP_Node_Secure *instance;

    void onReceive(const uint8_t *mac, const uint8_t *data, int len);

    uint32_t getNextSendSeq(const array<uint8_t, 6> &mac);
    void maybeFlushToNVS(const string &macKey, PeerState &state);

    function<void(const Pocket &)> rxCallback;

    bool addPeer(const array<uint8_t, 6> &mac);

    Preferences prefs;

    static string macToHexKey(const uint8_t *mac, const char *prefix);
};
