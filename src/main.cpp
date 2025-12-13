#include <Arduino.h>
#include "LHRP/LHRP.hpp"

/* optional test address */
Address a = {0, 1, 2};

/* create node with peers */
LHRP_Node net({
    {{0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x01}, {0, 1}},
    {{0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x02}, {0}},
    {{0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x03}, {0, 1, 2}},
});

void setup()
{
  Serial.begin(115200);
  delay(1000);

  net.begin();
}

void loop()
{
  Pocket p;
  p.address = {0, 1, 2};
  p.payload = {1, 2, 3, 4};

  net.send(p);
  delay(1000);
}
