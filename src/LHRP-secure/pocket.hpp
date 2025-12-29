#pragma once

#include <vector>
#include <Arduino.h>

using namespace std;

struct Address : public vector<uint8_t>
{
    // inherit vector constructors
    using vector<uint8_t>::vector;

    // initializer_list constructor
    Address(std::initializer_list<uint8_t> init)
        : vector<uint8_t>(init) {}
};

struct Pocket
{
    Address destAddress;
    Address srcAddress;
    vector<uint8_t> payload;
    bool errored;
};
