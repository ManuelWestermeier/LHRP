#include <Arduino.h>

#include "LHRP-secure/LHRP.hpp"

#define LED_BUILTIN 2 // Define built-in LED pin for ESP32

#include "get-node-configuration.hpp"

#define CVG networkConfiguration1

#define KEY {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}
#define NET_ID 111
/* create node with peers */
LHRP_Node_Secure net = getNodeSecure(NET_ID, KEY, CVG);
// LHRP_Node net = LHRP_Node(getNodeConfiguration(CVG));

void blink()
{
  // led test
  pinMode(LED_BUILTIN, OUTPUT);
  analogWrite(LED_BUILTIN, 200);
  delay(100);
  analogWrite(LED_BUILTIN, 0);
}

#define PIN_JOYSTICK_X 32
#define PIN_JOYSTICK_Y 33
#define PIN_JOYSTICK_BTN 25
#define POWER_PIN 34

void setup()
{
  Serial.begin(115200);
  Serial.println("LHRP Node Starting...");

  // joystick
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  pinMode(PIN_JOYSTICK_X, INPUT);
  pinMode(PIN_JOYSTICK_Y, INPUT);
  pinMode(PIN_JOYSTICK_BTN, INPUT_PULLUP);

  blink();

  Serial.println("Node Addresssize: " + String(net.node.you.size()));
  // print the esps mac
  Serial.print("MAC Address: {");
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] < 16)
      Serial.print("0");
    Serial.print("0x");
    Serial.print(mac[i], HEX);
    if (i < 5)
      Serial.print(", ");
  }
  Serial.println("}");

  delay(1000);

  net.onPocketReceive([&](const Pocket &pocket) { // handle received pocket
    if (pocket.errored)
      return;

    blink();
    Serial.println("Received pocket for Address:");
    Serial.println(pocket.destAddress.size());
    Serial.println(pocket.srcAddress.size());
    Serial.println(pocket.payload.size());
    if (!pocket.payload.empty())
    {
      analogWrite(LED_BUILTIN, pocket.payload[0]);
      Serial.println("LED brightness set to " + String(pocket.payload[0]));
    }
  });

  Serial.println(net.begin() ? "LHRP Node Started!" : "LHRP Node Failed to Start!");
}

void loop()
{
  if (!isSender())
  {
    delay(100);
    return;
  }

  static bool toggleState = false;
  static bool lastBtnState = HIGH;

  // --- Read joystick ---
  float x = ((analogRead(PIN_JOYSTICK_X)) - 2047.0f) / 2047.0f;
  float y = ((analogRead(PIN_JOYSTICK_Y)) - 2047.0f) / 2047.0f;

  x = constrain(x, -1.0f, 1.0f);
  y = constrain(y, -1.0f, 1.0f);

  uint8_t xValue = (uint8_t)(abs(x) * 255.0f);
  uint8_t yValue = (uint8_t)(abs(y) * 255.0f);

  // --- Button toggle ---
  bool btnState = digitalRead(PIN_JOYSTICK_BTN);
  if (btnState)
    Serial.println(x + String("|") + y);

  if (lastBtnState == HIGH && btnState == LOW)
  {
    toggleState = !toggleState;
  }
  lastBtnState = btnState;

  uint8_t toggleValue = toggleState ? 255 : 0;

  // --- Send to NODE 1 (|X|) ---
  {
    Address destAddress = CVG.node1;
    std::vector<uint8_t> payload(net.maxPayloadSize(destAddress), 0);
    payload[0] = xValue;
    Serial.println(net.send(destAddress, payload) ? "Send X" : "Error X");
  }

  // --- Send to NODE 2 (|Y|) ---
  {
    Address destAddress = CVG.node2;
    std::vector<uint8_t> payload(net.maxPayloadSize(destAddress), 0);
    payload[0] = yValue;
    Serial.println(net.send(destAddress, payload) ? "Send Y" : "Error Y");
  }

  // --- Send to NODE 3 (toggle) ---
  {
    Address destAddress = CVG.node3;
    std::vector<uint8_t> payload(net.maxPayloadSize(destAddress), 0);
    payload[0] = toggleValue;
    Serial.println(net.send(destAddress, payload) ? "Send Toggle" : "Error Toggle");
  }

  delay(500);
}