#pragma once

#include <vector>
#include <algorithm>
#include "pocket.hpp"

#define LHRP_PIN_ERROR 255

using namespace std;

struct Match
{
    uint16_t positive;
    uint16_t negative;
};

inline Match match(const Address &connection, const Address &pocket)
{
    size_t minLen = min(connection.size(), pocket.size());
    Match m{0, 0};

    while (m.positive < minLen &&
           connection[m.positive] == pocket[m.positive])
        m.positive++;

    m.negative = connection.size() - m.positive;
    return m;
}

inline int matchIndex(const Match &m)
{
    return (int)m.positive - (int)m.negative;
}

inline bool eq(const Address &a1, const Address &a2)
{
    return a1.size() == a2.size() &&
           equal(a1.begin(), a1.end(), a2.begin());
}

inline bool isChildren(const Address &other, const Address &you)
{
    if (other.size() <= you.size())
        return false;

    for (size_t i = 0; i < you.size(); i++)
        if (other[i] != you[i])
            return false;

    return true;
}

struct Connection
{
    Address address;
    uint8_t pin;
};

struct Node
{
    vector<Connection> connections;
    Address you;

    uint8_t send(const Pocket &p)
    {
        if (eq(you, p.destAddress))
            return 0;

        if (connections.empty())
            return LHRP_PIN_ERROR;

        Connection best = connections[0];
        int bestIdx = matchIndex(match(best.address, p.destAddress));
        size_t bestLen = best.address.size();

        for (size_t i = 1; i < connections.size(); i++)
        {
            int idx = matchIndex(match(connections[i].address, p.destAddress));
            size_t len = connections[i].address.size();

            if (idx > bestIdx || (idx == bestIdx && len > bestLen))
            {
                best = connections[i];
                bestIdx = idx;
                bestLen = len;
            }
        }

        // wen child nicht vorhanden ist
        bool directChild = isChildren(p.destAddress, you);
        int ownMatchIdx = matchIndex(match(you, p.destAddress));
        if (directChild && (!isChildren(best.address, you) || bestIdx < ownMatchIdx))
            return 0;

        // wenn parent nicht vorhanden ist
        if (bestIdx <= ownMatchIdx)
            return LHRP_PIN_ERROR;

        return best.pin;
    }
};
