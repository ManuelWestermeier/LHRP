#pragma once
#include <vector>
#include <algorithm>
#include "pocket.hpp"

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
        if (connections.empty())
            return 0;
        if (eq(you, p.address))
            return 0;

        Connection best = connections[0];
        int bestIdx = matchIndex(match(best.address, p.address));
        size_t bestLen = best.address.size();

        bool directChild = isChildren(p.address, you);

        for (size_t i = 1; i < connections.size(); i++)
        {
            int idx = matchIndex(match(connections[i].address, p.address));
            size_t len = connections[i].address.size();

            if (idx > bestIdx || (idx == bestIdx && len > bestLen))
            {
                best = connections[i];
                bestIdx = idx;
                bestLen = len;
            }
        }

        if (directChild && !isChildren(best.address, you))
            return 0;

        return best.pin;
    }
};
