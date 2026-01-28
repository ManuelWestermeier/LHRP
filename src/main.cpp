#include <Arduino.h>
#include <array>
#include "LHRP-secure/LHRP.hpp"
#include "get-node-configuration.hpp"

#define LED_BUILTIN 2

#define PIN_JOYSTICK_X 32
#define PIN_JOYSTICK_Y 33
#define PIN_JOYSTICK_BTN 25
// CHANGED: use an output-capable GPIO to power the joystick
#define POWER_PIN 26

#define NET_ID 111
#define CVG networkConfiguration1

std::array<uint8_t, 16> KEY = {0x01, 0x02, 0x03, 0x04,
                               0x05, 0x06, 0x07, 0x08,
                               0x09, 0x0A, 0x0B, 0x0C,
                               0x0D, 0x0E, 0x0F, 0x10};

LHRP_Node_Secure net = getNodeSecure(NET_ID, KEY, CVG);

// -------------------- LED PWM Setup --------------------
const int ledChannel = 0;
const int ledFreq = 5000;
const int ledResolution = 8; // 0-255

void setupPWM()
{
  ledcSetup(ledChannel, ledFreq, ledResolution);
  ledcAttachPin(LED_BUILTIN, ledChannel);
}

void setLed(uint8_t brightness)
{
  ledcWrite(ledChannel, brightness);
}

// -------------------- Blink helper --------------------
void blink()
{
  setLed(200);
  delay(100);
  setLed(0);
}

// -------------------- Setup --------------------
void setup()
{
  Serial.begin(115200);
  Serial.println("LHRP Node Starting...");

  // Power on joystick - use a real output pin (GPIO34 is input-only and cannot drive)
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);

  // analog pins do not require pinMode but INPUT is OK
  pinMode(PIN_JOYSTICK_X, INPUT);
  pinMode(PIN_JOYSTICK_Y, INPUT);
  pinMode(PIN_JOYSTICK_BTN, INPUT_PULLUP);

  // LED PWM
  setupPWM();
  blink();

  // Node info
  Serial.println("Node Addresssize: " + String(net.node.you.size()));

  // Print ESP32 MAC
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

  // Receive callback
  net.onPocketReceive([&](const Pocket &pocket)
                      {
        if (pocket.errored) return;
        // blink();
        if (!isSender()) {
            Serial.println("Received pocket:");
            Serial.println("Dest size: " + String(pocket.destAddress.size()));
            Serial.println("Src size: " + String(pocket.srcAddress.size()));
            Serial.println("Payload size: " + String(pocket.payload.size()));
        }
        if (!pocket.payload.empty()) {
            // Set LED brightness from first byte
            setLed(pocket.payload[0]);
            if (!isSender())
                Serial.println("LED brightness set to " + String(pocket.payload[0]));
        } });

  Serial.println(net.begin() ? "LHRP Node Started!" : "LHRP Node Failed to Start!");
}

// -------------------- Loop --------------------
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
  int rawX = analogRead(PIN_JOYSTICK_X); // 0-4095 on ESP32 (12-bit)
  int rawY = analogRead(PIN_JOYSTICK_Y);

  // Debug: print raw ADC values to verify joystick is powered and working
  Serial.println("Raw ADC: X=" + String(rawX) + " Y=" + String(rawY));

  float x = ((float)rawX - 2048.0f) / 2048.0f;
  float y = ((float)rawY - 2048.0f) / 2048.0f;

  x = constrain(x, -1.0f, 1.0f);
  y = constrain(y, -1.0f, 1.0f);

  uint8_t xValue = (uint8_t)(abs(x) * 255.0f);
  uint8_t yValue = (uint8_t)(abs(y) * 255.0f);

  // --- Button toggle ---
  bool btnState = digitalRead(PIN_JOYSTICK_BTN);
  if (lastBtnState == HIGH && btnState == LOW)
  {
    toggleState = !toggleState;
  }
  lastBtnState = btnState;
  uint8_t toggleValue = toggleState ? 255 : 0;

  Serial.println("Captured values:");
  Serial.println("Button: " + String(btnState));
  Serial.println("X|Y: " + String(xValue) + "|" + String(yValue));

  // --- Send to NODE 1 (button toggle) ---
  {
    Address destAddress = CVG.node1;
    std::vector<uint8_t> payload(net.maxPayloadSize(destAddress), 0);
    if (!payload.empty())
      payload[0] = toggleValue;
    Serial.println(net.send(destAddress, payload) ? "Send Toggle" : "Error Toggle");
  }

  // --- Send to NODE 2 (X-axis brightness) ---
  {
    Address destAddress = CVG.node2;
    std::vector<uint8_t> payload(net.maxPayloadSize(destAddress), 0);
    if (!payload.empty())
      payload[0] = xValue;
    Serial.println(net.send(destAddress, payload) ? "Send X" : "Error X");
  }

  // --- Send to NODE 3 (Y-axis brightness) ---
  {
    Address destAddress = CVG.node3;
    std::vector<uint8_t> payload(net.maxPayloadSize(destAddress), 0);
    if (!payload.empty())
      payload[0] = yValue;
    Serial.println(net.send(destAddress, payload) ? "Send Y" : "Error Y");
  }

  delay(500);
}
