# LHRP – Lightweight Hierarchical Routing Protocol

LHRP is a lightweight, hierarchical routing protocol for **ESP32**
based on **ESP-NOW**.  
It enables forwarding packets (“Pockets”) between nodes using **address prefixes**
without central coordination.

---

## Features

- ESP-NOW based communication (no WiFi AP required)
- Hierarchical address-based routing
- Automatic multi-hop forwarding
- Very low overhead (fixed-size packets)
- Deterministic routing decisions
- Suitable for mesh-like embedded networks

---

## Core Concepts

### Address

An address is a vector of `uint8_t` values:

```cpp
using Address = vector<uint8_t>;
```

Examples:

```
{1}
{1,1}
{1,1,1}
```

Longer addresses represent deeper hierarchy levels.

---

### Pocket

A Pocket represents a logical packet:

```cpp
struct Pocket {
    Address address;
    vector<uint8_t> payload;
};
```

---

### Routing Logic

Routing is based on:

1. Longest common prefix match
2. Best positive/negative match index
3. Child routing rule

Implemented in:

```cpp
uint8_t Node::send(const Pocket& p);
```

Return values:

- `0` → packet is for this node
- `>0` → forward via peer pin

---

## LHRP_Node Usage

### Initialization

```cpp
LHRP_Node net({
    { selfMac, myAddress },
    { peerMac1, peerAddress1 },
    { peerMac2, peerAddress2 }
});
```

First peer = self, others = connections.

---

### Start Node

```cpp
net.begin();
```

Initializes WiFi, ESP-NOW, registers callbacks and peers.

---

### Send Packet

```cpp
net.send(Pocket{ targetAddress, payload });
```

Routing automatically determines the next hop.

---

### Receive Packet

```cpp
net.onPocketReceive([](const Pocket& p){
    // handle packet
});
```

---

## ESP-NOW Transport

Packets are sent as fixed-size `RawPacket` structures for speed and efficiency.

---

## Example

Random LED brightness is sent between nodes using hierarchical routing.

```cpp
net.send(Pocket{ node2Address, { random(255) } });
```

---

## Project Structure

```
LHRP/
├── src/
├── LHRP/
├── platformio.ini
└── README.md
```

---

## Requirements

- ESP32
- Arduino framework
- PlatformIO
- ESP-NOW enabled

---

## Notes

- No encryption enabled
- Static peer configuration
- MAC addresses must be known

---

## License

Creative Common
