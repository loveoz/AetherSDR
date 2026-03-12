# AetherSDR Linux Native Client

A Linux-native SmartSDR-compatible client for FlexRadio Systems transceivers,
built with **Qt6** and **C++20**.

Current version: **0.1.1**

---

## Features

| Feature | Status |
|---|---|
| UDP discovery (port 4992) | ✅ |
| TCP command/control connection | ✅ |
| SmartSDR protocol parser (V/H/R/S/M) | ✅ |
| Multi-word status object parsing (`slice 0`, `display pan 0x...`) | ✅ |
| Slice model (frequency, mode, filter) | ✅ |
| Frequency dial — click top/bottom half to tune up/down | ✅ |
| Frequency dial — scroll wheel tuning | ✅ |
| Frequency dial — direct keyboard entry (double-click) | ✅ |
| GUI↔radio frequency sync (no feedback loop) | ✅ |
| Mode selector (USB/LSB/CW/AM/FM/DIG…) | ✅ |
| Panadapter VITA-49 UDP stream receiver | ✅ |
| Panadapter spectrum widget (FFT bins) | ✅ |
| Panadapter dBm range auto-calibrated from radio | ✅ |
| Audio RX via VITA-49 UDP + Qt Multimedia | ✅ |
| Audio TX (microphone → radio) | ✅ |
| Volume / mute control | ✅ |
| TX button | ✅ |
| Persistent window geometry | ✅ |

---

## Architecture

```
src/
├── main.cpp
├── core/
│   ├── RadioDiscovery.h/.cpp    # UDP broadcast listener (port 4992)
│   ├── RadioConnection.h/.cpp   # TCP command channel + heartbeat
│   ├── CommandParser.h/.cpp     # Stateless line parser/builder
│   ├── PanadapterStream.h/.cpp  # VITA-49 UDP FFT receiver
│   └── AudioEngine.h/.cpp       # VITA-49 UDP → Qt Multimedia audio
├── models/
│   ├── RadioModel.h/.cpp        # Central radio state, owns connection
│   └── SliceModel.h/.cpp        # Per-slice receiver state
└── gui/
    ├── MainWindow.h/.cpp        # Main application window
    ├── FrequencyDial.h/.cpp     # Custom 9-digit frequency widget
    ├── ConnectionPanel.h/.cpp   # Radio list + connect/disconnect
    └── SpectrumWidget.h/.cpp    # Panadapter display (FFT bins)
```

### Data flow

```
                   ┌─────────────────────┐
 UDP bcast (4992)  │   RadioDiscovery    │
 ──────────────────▶   (QUdpSocket)      │──▶ ConnectionPanel (GUI)
                   └─────────────────────┘

 TCP (4992)        ┌─────────────────────┐
 ──────────────────▶   RadioConnection   │──▶ RadioModel ──▶ SliceModel ──▶ GUI
 Commands ◀────────   (QTcpSocket)       │
                   └─────────────────────┘

 UDP VITA-49       ┌─────────────────────┐
 ──────────────────▶  PanadapterStream   │──▶ SpectrumWidget (FFT display)
                   └─────────────────────┘

 UDP audio         ┌─────────────────────┐
 ──────────────────▶   AudioEngine       │──▶ QAudioSink (speakers)
 Mic audio ◀───────   (VITA-49 framing)  │◀── QAudioSource (microphone)
                   └─────────────────────┘
```

---

## Building

### Dependencies

```bash
# Arch / CachyOS
sudo pacman -S qt6-base qt6-multimedia cmake ninja pkgconf

# Ubuntu 24.04+
sudo apt install qt6-base-dev qt6-multimedia-dev cmake ninja-build pkg-config
```

### Configure & build

```bash
git clone https://github.com/ten9876/AetherSDR.git
cd AetherSDR
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
./build/AetherSDR
```

---

## SmartSDR Protocol Notes (firmware v1.4.0.0)

### Message format

| Prefix | Direction | Example |
|--------|-----------|---------|
| `V`    | Radio→Client | `V1.4.0.0` — firmware version |
| `H`    | Radio→Client | `H479F0832` — assigned client handle |
| `C`    | Client→Radio | `C1\|sub slice all` — command |
| `R`    | Radio→Client | `R1\|0\|` — response (0 = OK) |
| `S`    | Radio→Client | `S479F0832\|slice 0 RF_frequency=14.100000 mode=USB` |
| `M`    | Radio→Client | `M479F0832\|<encoded message>` |

### Status object parsing

Status object names are **multi-word**: `"slice 0"`, `"display pan 0x40000000"`,
`"interlock band 9"`. The parser finds the split point by locating the last space
before the first `=` sign in the status body — this correctly separates the object
name from the key=value pairs.

### Connection sequence (SmartConnect / standalone)

```
TCP connect to radio:4992
  ← V<version>
  ← H<handle>               (client handle assigned)
  → C|sub slice all
  → C|sub tx all
  → C|sub atu all
  → C|sub meter all
  → C|client gui            (required before panadapter/slice creation)
  → C|client program AetherSDR
  [bind UDP socket, send registration packet to radio:4992]
  → C|client set udpport=<port>   (returns 50001000 on v1.4.0.0 — expected)
  → C|slice list
    ← R|0|0                 (SmartConnect: existing slice IDs)
    → C|slice get 0
  ← S|...|slice 0 RF_frequency=14.100000 ...   (subscription push)
  ← S|...|display pan 0x40000000 center=14.1 bandwidth=0.2 min_dbm=-135 max_dbm=-40 ...
```

### Firmware v1.4.0.0 quirks

- **`client set udpport`** returns error `0x50001000` ("command not supported").
  UDP port registration must use the one-byte UDP packet method: bind a local UDP
  socket, send a single `\x00` byte to `radio:4992`, and the radio learns our
  IP:port from the datagram source address.
- **`display panafall create`** returns `0x50000016` on this firmware — use
  `panadapter create` instead.
- **Slice frequency** is reported as `RF_frequency` (not `freq`) in status messages.
- **Panadapter bins** are **unsigned uint16**, linearly mapped:
  `dbm = min_dbm + (sample / 65535.0) × (max_dbm - min_dbm)`.
  The `min_dbm` / `max_dbm` values are broadcast in the `display pan` status message.
- **VITA-49 packet type**: top byte `0x38` = panadapter FFT (Extension Data);
  top byte `0x18` = audio/IF (skip these in the panadapter stream).
- **Panadapter stream ID**: `0x04000009` (not `0x40000000` — that is the pan *object* ID).

### GUI↔radio frequency sync

`SliceModel` setters (`setFrequency`, `setMode`, etc.) emit `commandReady`
immediately, which `RadioModel` routes to `RadioConnection::sendCommand`.
`MainWindow` uses an `m_updatingFromModel` guard flag to prevent echoing
model-driven dial updates back to the radio.

---

## Next Steps

- [ ] Waterfall display (scrolling `QImage` below the spectrum)
- [ ] Slice filter passband shading on the spectrum
- [ ] Multi-slice support (slice tabs or overlaid markers)
- [ ] Audio RX via VITA-49 stream (pipe to `AudioEngine`)
- [ ] Band stacking / band map

---

## Contributing

PRs welcome. See the modular architecture above — each subsystem is independent
and can be developed/tested in isolation.
