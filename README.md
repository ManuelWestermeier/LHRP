# LHRP_Node_Secure – Secure Hierarchical Routing over ESP-NOW

## Überblick

**LHRP_Node_Secure** ist eine sichere, hierarchische Routing- und Kommunikationsschicht für ESP32-Geräte auf Basis von **ESP-NOW**.  
Sie implementiert das **LHRP (Lightweight Hierarchical Routing Protocol)** mit:

- hierarchischen Adressen
- automatischem Routing (Parent/Child)
- **AES-128-GCM Verschlüsselung**
- **Replay-Schutz mit persistenten Sequenznummern**
- netzwerkweiter Kanalableitung aus `netId`
- Speicherung kritischer Zustände in **NVS (Preferences)**

Das System ist vollständig **peer-to-peer**, benötigt keinen Access Point und ist für Mesh-ähnliche Topologien geeignet.

---

## Eigenschaften

- 🔐 **Ende-zu-Ende-Verschlüsselung** (AES-GCM, 128 Bit)
- 🛡 **Replay-Schutz** (persistente Sequenznummern pro Peer)
- 🌳 **Hierarchische Adressierung** (Tree / Prefix Routing)
- 📡 **ESP-NOW-basiert** (kein WiFi-AP notwendig)
- 💾 **NVS-gesichert** (Sequenzen über Neustarts hinweg gültig)
- ⚡ **Deterministisches Routing** (Longest Prefix Match)

---

## Architektur

### Adressen

Eine Adresse ist ein Vektor aus Bytes:

```cpp
Address a = {1, 2, 3};
```

- Hierarchisch (Prefix-basiert)
- Elternknoten besitzen kürzere Präfixe
- Kinder erben das Präfix des Elternknotens

Beispiel:

- `{1}` → Root
- `{1,2}` → Child
- `{1,2,7}` → Leaf

---

### Routing (Node)

Das Routing basiert auf:

- **Longest Prefix Match**
- Parent-/Child-Erkennung
- Fallback-Logik bei fehlenden Routen

Die Entscheidung erfolgt über `Node::send(const Pocket&)` und liefert:

- `0` → lokal zustellen
- `pin > 0` → Weiterleitung über Peer
- `LHRP_PIN_ERROR` → keine Route

---

## Sicherheit

### Verschlüsselung

- Algorithmus: **AES-128-GCM**
- IV: 96 Bit (zufällig)
- Tag: 128 Bit
- AAD (authentifiziert, aber unverschlüsselt):
  - `netId`
  - `lengths`
  - `dataLen`

### Replay-Schutz

- Jede Verbindung nutzt eine **monoton steigende Sequenznummer**
- Gespeichert in NVS:
  - `s_<MAC>` → letzte gesendete Sequenz
  - `r_<MAC>` → letzte empfangene Sequenz

- Pakete mit `seq <= lastSeen` werden verworfen

---

## Netzwerk & Kanalwahl

Der WiFi-Kanal wird **deterministisch** aus der `netId` berechnet:

```cpp
channel = (netId * 7 % 13) + 1;
```

➡ Gleiche `netId` ⇒ gleicher Kanal
➡ Unterschiedliche Netze interferieren weniger

---

## RawPacket-Format (250 Bytes)

```
| netId | lengths | dataLen | IV (12) | TAG (16) | encrypted payload |
```

Payload (verschlüsselt):

```
| seq (4) | destAddr | srcAddr | payload |
```

---

## Verwendung

### Initialisierung

```cpp
array<uint8_t,16> key = { /* 16-byte AES key */ };

LHRP_Node_Secure node(
    1,              // netId
    key,
    {
        {macSelf,   {1}},
        {macPeer1,  {1,2}},
        {macPeer2,  {1,3}}
    }
);

node.begin();
```

> **Wichtig:**
> Der **erste Peer** in der Liste ist immer der **eigene Knoten**.

---

### Senden

```cpp
Address dest = {1,2};
vector<uint8_t> data = {0xAA, 0xBB};

node.send(dest, data);
```

---

### Empfangen

```cpp
node.onPocketReceive([](const Pocket& p){
    // p.srcAddress
    // p.destAddress
    // p.payload
});
```

---

### Maximale Payload-Größe

```cpp
int maxSize = node.maxPayloadSize(dest);
```

Abhängig von:

- Adresstiefen
- RawPacket-Größe
- AES-GCM Overhead

---

## Speicher (NVS)

Namespace: **`"lhrp"`**

Gespeicherte Keys:

- `s_<MACHEX>` → letzte gesendete Sequenz
- `r_<MACHEX>` → letzte empfangene Sequenz

Beispiel:

```
s_AABBCCDDEEFF
r_AABBCCDDEEFF
```

---

## Abhängigkeiten

- ESP32 Arduino Core
- `esp_now`
- `mbedtls`
- `Preferences`
- `WiFi`

---

## Einschränkungen

- Max. Adresstiefe: **15**
- Max. RawPacket-Größe: **250 Bytes**
- AES-Key ist **pre-shared**
- Kein dynamisches Peer-Discovery

---

## Zielgruppe

- Sichere ESP32-Mesh-Netze
- Sensornetze
- Steuerungs- und Aktor-Netze
- Offline-Kommunikation
- Embedded Security-Anwendungen

---

## Status

**Produktionsreif**
Design ist deterministisch, speichersicher und reboot-resistent.

---

#### Disclaimer

Projekt von _Manuel Westermeier_ gecoded, Dokumentation von ChatGPT
