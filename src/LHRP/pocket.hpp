#pragma once

#include <vector>
#include <Arduino.h>

using namespace std;

struct Address : public vector<uint16_t>
{
    // inherit vector constructors
    using vector<uint16_t>::vector;

    // initializer_list constructor
    Address(std::initializer_list<uint16_t> init)
        : vector<uint16_t>(init) {}
};

struct Pocket
{
    Address address;
    vector<uint8_t> payload;
};
