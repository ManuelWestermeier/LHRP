# Methodik des LHRP (Lightweight Hierarchical Routing Protocol)

## 1. Grundlegende Überlegungen

Das Ziel von LHRP ist der effiziente Datentransport mit minimalem Metadaten-Overhead bei gleichzeitiger Wahrung von Sicherheit, Dynamik und Skalierbarkeit.

In LHRP sind alle Nodes grundsätzlich gleichgestellt (keine festen Rollen). Da klassische Netz-Topologien spezifische Nachteile für dieses Szenario aufweisen, wurde ein hybrider Ansatz gewählt:

### Vergleich der Netzwerk-Topologien

| Topologie | Nachteile im LHRP-Kontext |

| :--- | :--- |

| **Vollvermascht** | Hoher Speicherbedarf und ineffizientes Routing, da jede Node die gesamte Struktur kennen muss. |

| **Bus** | Zu unflexibel und schlecht skalierbar bei dynamischen Änderungen. |

| **Stern** | Zentraler Flaschenhals und ein kritischer Single Point of Failure. |

### Die LHRP-Lösung

LHRP nutzt eine **baumartige bzw. hierarchische Topologie**, die Folgendes kombiniert:

- **Strikt hierarchische Node-Adressen** (logische Baumstruktur).

- **Frei vernetzte physische Verbindungen** (z. B. Shortcut-Links).

**Wesentliche Regel:** Jede Node muss mindestens eine Verbindung zu ihrer Parent-Adresse besitzen (Ausnahme: Root-Node). Dadurch entsteht eine logische Hierarchie, ohne die physische Vernetzung einzuschränken.

## 2. Match-Index-Berechnung (Routing-Entscheidung)

LHRP fungiert als Forward-Protokoll. Beim Eintreffen eines Pakets wird der beste **Next Hop** anhand folgender Logik ermittelt:

1.  **Zielprüfung:** Ist das Paket an die eigene Node adressiert?
    - _Ja:_ Übergabe an die Applikation (kein Forwarding).

    - _Nein:_ Vergleich aller verbundenen Nodes.

2.  **Bewertung:** Die Entscheidung basiert auf dem **Match Index**.

## 3. Der Match Index

Der Match Index bewertet die Übereinstimmung einer potenziellen Next-Hop-Adresse mit der Zieladresse des Pakets. Er setzt sich aus zwei Komponenten zusammen:

- **Positive:** Anzahl der identischen Bytes ab Beginn der Adresse (Adressen sind In Layers gegliedert, die jeweils in bytes representiert werden Bsp: vector<uint8_t>{2, 255, 4, 5}. Layer 3 = 4) (Prefix-Match).

- **Negative:** Anzahl der verbleibenden Bytes der Next-Hop-Adresse (negative = nextHopAddress.length - positive).

### Formel

MatchIndex = positive - negative

### Interpretation

- **Maximaler Match Index:** Vollständiger Match mit der Zieladresse (positive = Zieladresse.length).

- **Minimaler Match Index:** Kein gemeinsames Prefix (- nextHopAddress.length).

**Routing-Verhalten:**

- **Negative Indizes:** Bevorzugen Parent-Nodes (Aufstieg im Baum).

- **Positive Indizes:** Bevorzugen Child-Nodes (Abstieg im Baum).

- **Shortcut-Verbindungen:** Werden durch den Index automatisch gewichtet und bevorzugt, wenn sie den Weg verkürzen.

## 4. Auswahl des besten Next Hops

Die Auswahl erfolgt nach strikten Prioritäten:

1.  **Höchster Index:** Der Next Hop mit dem höchsten Match Index gewinnt.

2.  **Gleichstand (Tie-Break):** Bei identischem Index gewinnt die Verbindung mit der **längeren Adresse** verwendet (entlastet Root-Nodes, fördert tieferes Routing).

3.  **Lokaler Stopp:** Hat die eigene Node den besten Match Index, erfolgt die lokale Zustellung.

4.  **Hierarchie-Schutz:** Wenn das Ziel ein direktes Child der eigenen Node ist, darf das Paket nicht an einen Parent oder Neighbor gesendet werden.

## 5. Codebeispiel (C++)

### Datenstrukturen und Match-Logik

```cpp
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
```

## Adressvergleiche

```C++
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
```

## Routing-Entscheidung

```C++
uint8_t send(const Pocket &p)
{
    if (eq(you, p.destAddress))
        return 0;

    if (connections.empty())
        return LHRP_PIN_ERROR;

    Connection *best = &connections[0];
    int bestIdx = matchIndex(match(best->address, p.destAddress));
    size_t bestLen = best->address.size();

    for (size_t i = 1; i < connections.size(); i++)
    {
        int idx = matchIndex(match(connections[i].address, p.destAddress));
        size_t len = connections[i].address.size();

        if (idx > bestIdx || (idx == bestIdx && len > bestLen))
        {
            best = &connections[i];
            bestIdx = idx;
            bestLen = len;
        }
    }

        // wen child nicht vorhanden ist
    bool directChild = isChildren(p.destAddress, you);
    int ownMatchIdx = matchIndex(match(you, p.destAddress));
    if (directChild && (!isChildren(best->address, you) || bestIdx < ownMatchIdx))
        return 0;

    // wenn parent nicht vorhanden ist
    if (bestIdx <= ownMatchIdx)
        return LHRP_PIN_ERROR;

    return best->pin;
}
```
