# PPG Sensor Platform

End-to-end firmware and analysis toolkit for photoplethysmography (PPG) using a MAX30105 sensor, an LIS3DH accelerometer, and a Parent/Child two-node workflow.

This repository contains:
- Firmware (Arduino/C++) for both Parent and Child devices, built from a shared codebase.
- Signal processing (MATLAB and Python) to turn raw IR samples into usable features and plots.
- Design documents (SDS and appendix listings) describing architecture, timing, and the exported data format.

---
## Table of Contents
- [Overview](#overview)
- [Architecture](#architecture)
- [Repository Layout](#repository-layout)
- [Hardware](#hardware)
- [Firmware](#firmware)
  - [Dependencies](#dependencies)
  - [Build Options](#build-options)
  - [Configuration Knobs](#configuration-knobs)
  - [Build & Flash](#build--flash)
  - [Run Sequence](#run-sequence)
- [Data Format](#data-format)
- [Signal Processing](#signal-processing)
  - [Python](#python)
  - [MATLAB](#matlab)
- [Troubleshooting & Notes](#troubleshooting--notes)
- [License](#license)

---
## Overview
- **Goal:** Capture IR PPG waveform segments on a low-power node, detect movement with an accelerometer, synchronize timestamps across a Parent/Child pair, and export clean CSV for offline analysis.
- **Sync:** A one-wire wired sync pulse from Parent to Child aligns timebases; any remaining offset is applied by the Child during CSV export.
- **Metadata:** Each capture logs device configuration (LED current, pulse width, sampling rate, etc.) before waveform rows.

---
## Architecture

### Roles
**Parent**
- Drives the sync pulse.
- Waits for a single tap to start measuring.
- Captures a segment and exports CSV with *local* timestamps.

**Child**
- Listens for the sync pulse.
- Computes offset to the Parent timebase.
- Waits for a tap, captures, and exports CSV **with offset applied** to align with Parent time.

### Key Modules
**SystemShared.h / SystemShared.cpp**
- Shared declarations and implementations for Parent and Child.
- I²C helpers: `writeToReg`, `i2cRead`, `i2cUpdateBits`, `i2cSetBit`.
- Sync protocol: `ParentWiredSyncProtocol`, `ChildWiredSyncProtocol`.
- RGB status helpers: `red()`, `yellow()`, etc.
- Role aliases via `#define ROLE_PARENT` or `#define ROLE_CHILD` so both sketches can call generic names (`sleepMode`, `waitMode`, `measureMode`, `WiredSyncProtocol`, `SyncFunc`).

**PPGHandler.h / PPGHandler.cpp**
- PPG sampling and buffering.
- `waveformPkg` linked-list buffer with fixed-size nodes.
- `enqueue()`, `dequeue()`, `dequeueToCSV()`.
- Metadata via `setInternals()` and `printInternals()`.
- Timebase in **0.1 ms ticks** via `currentTime()` (rounded from `micros()`).

**AccHandler**
- LIS3DH configuration (single/double tap, AOI/threshold motion, sleep/wake).
- Minimal `isr()` that raises a `volatile` movement flag.

---
## Repository Layout
```
Embedded-dev/
  src/
    IncludeFiles/
      Helpers.h  
      Helpers.cpp
      PPGHandler.h
      PPGHandler.cpp
      AccHandler.h
      AccHandler.cpp
  ArduinoMains/              
    PPG_Wearable_Parent.ino # Parent sketch (defines ROLE_PARENT)
    PPG_Wearable_Child.ino # Child sketch  (defines ROLE_CHILD)
  Deprecated_Versions/
Signal-Processing/
Python_Preprocessing/
  Test_Data/
parse_waveform.py

```


---
## Hardware
- **Sensors**
  - SparkFun MAX30105 (IR PPG; firmware sets IR mode)
  - ST LIS3DH (I²C), INT1 routed to MCU GPIO
- **I²C**
  - `Wire` at 400 kHz (FAST)
  - MAX30105 default address: `0x57`
  - LIS3DH default address: `0x18` (use `0x19` if SA0 is HIGH)
- **Pins (default macros; override per board if needed)**
  - `INTERRUPT_PIN`  → accelerometer INT1 (default `D2`)
  - `EXTERNAL_INTERRUPT` → wired sync sense (default `D3`)
  - `INTERRUPTER`    → wired sync drive (Parent) (default `D4`)
  - RGB: supports either (`LEDR`,`LEDG`,`LEDB`) or (`LED_RED`,`LED_GREEN`,`LED_BLUE`), else falls back to `LED_BUILTIN`.

---
## Firmware

### Dependencies
Install via Arduino Library Manager (or add to PlatformIO `lib_deps`):
- SparkFun MAX3010x Sensor Library (for MAX30105)
- Adafruit LIS3DH
- Adafruit Unified Sensor

### Build Options
- **Target cores:** Arduino ESP32 / AVR (others may work)
- **Recommended flags (often default in Arduino toolchains):**
  ```
  -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections
  ```
  Removes unused code and trims binary size.

### Configuration Knobs (in `PPGHandler.h`)
```cpp
#define BRIGHTNESS   170   // 0..255 (~0..50 mA)
#define SAMPLEAVE    1     // on-sensor averaging
#define LEDMODE      2     // IR only (per mapping)
#define SAMPLERATE   200   // Hz
#define PULSEWIDTH   215   // µs equivalent
#define ADCRANGE     16384 // nA full-scale
#define TIME         3     // seconds per capture
```

Timebase utility (rounded 0.1 ms ticks):
```cpp
inline unsigned long currentTime(){ return (MICROS() + 50u) / 100u; }
```

### Build & Flash

**Arduino IDE (per sketch)**
1. Open `firmware/parent/parent.ino` (or `firmware/child/child.ino`).
2. At the very top of the sketch, define the role **before** including the shared header:
   ```cpp
   #define ROLE_PARENT 1       // or: #define ROLE_CHILD 1
   #include "../SystemShared.h"
   ```
3. Select the board/port, install dependencies, and Upload.

**PlatformIO**
- Create two environments (`parent`, `child`) with:
  - `build_flags = -DROLE_PARENT` (or `-DROLE_CHILD`)
  - `lib_deps` for the sensor libraries listed above.

### Run Sequence

**Parent**
1. Sleep — LIS3DH **double-tap** armed, PPG in sleep.
2. Wait — Parent emits sync pulse (via `INTERRUPTER`), LIS3DH **single-tap** armed, PPG wake.
3. Measure — On tap, Parent captures PPG and dumps CSV (local timestamps).

**Child**
1. Sleep — LIS3DH sleep, PPG sleep.
2. Wait — Child listens for sync pulse, captures `child_timebase`, receives `parent_timebase` over Serial, computes `child_time_offset`, PPG wake.
3. Measure — On tap, Child captures and dumps CSV **with offset applied** to align with Parent time.

---
## Data Format
CSV emitted by `waveformPkg::dequeueToCSV(...)` (via `Serial`):
```
**Begin Metadata**
key, value
Run ID, <u8>
Brightness, <u16>
Sample Average, <u8>
Led Mode, <u8>
Sample Rate, <u16>
Pulse Width, <u16>
ADC range, <u16>
**Begin Waveform**
Time, Waveform
<t0>,<v0>
<t1>,<v1>
...
```
- **Time units:** 0.1 ms ticks since program start.  
  - Parent prints local `time`.  
  - Child prints `time ± child_time_offset` depending on `sign` so both series overlay.

---
## Signal Processing

### Python

**Environment**
```bash
python -m venv .venv
source .venv/bin/activate           # Windows: .venv\Scripts\activate
pip install -U numpy pandas scipy matplotlib
```

**Example usage**
```bash
python processing/python/analyze_ppg.py data/ppg_child.csv
```

**Reference sketch (`analyze_ppg.py`)**
```python
import pandas as pd, numpy as np, matplotlib.pyplot as plt
from io import StringIO

path = "data/ppg_child.csv"
text = open(path).read()
if "**Begin Waveform**" in text:
    wave = text.split("**Begin Waveform**", 1)[1].strip().splitlines()
    df = pd.read_csv(StringIO("\n".join(wave)), skiprows=1, names=["Time","Waveform"])
else:
    df = pd.read_csv(path)

df["t_s"] = df["Time"] * 1e-4
y = df["Waveform"].astype(float).to_numpy()
y = y - np.median(y)
y_filt = pd.Series(y).rolling(5, center=True, min_periods=1).mean().to_numpy()

plt.plot(df["t_s"], y_filt, label="IR (smoothed)")
plt.xlabel("Time (s)"); plt.ylabel("Amplitude (a.u.)"); plt.legend(); plt.title("PPG IR")
plt.show()
```

> Extend with band-pass (e.g., 0.5–8 Hz), peak detection, HR/HRV metrics, motion artifact flags, and multi-node alignment.

### MATLAB

**Quick import & plot**
```matlab
function T = import_ppg_csv(fname)
    raw = fileread(fname);
    parts = split(raw, '**Begin Waveform**');
    wave = strtrim(parts{end});
    T = readtable(string(wave), 'Delimiter', ',', 'HeaderLines', 1, ...
                  'ReadVariableNames', true);
    T.t_s = T.Time * 1e-4; % 0.1 ms ticks -> seconds
end

T = import_ppg_csv('data/ppg_parent.csv');
plot(T.t_s, T.Waveform); xlabel('Time (s)'); ylabel('Amplitude'); title('PPG IR');
```

> Add preprocessing (`detrend`, `bandpass`), `findpeaks`, and metrics (BPM, inter-beat intervals). Include artifact rejection using accelerometer events if desired.

---
## Troubleshooting & Notes
- **INT1 latch clearing (LIS3DH):** In tap modes the INT1 line is latched. Clear after the ISR by reading `CLICK_SRC`. For latched AOI motion routes, read `INT1_SRC`. Ensure the main loop reads the appropriate source after handling the interrupt flag.
- **Time wrap:** `micros()` wraps. Using 0.1 ms ticks reduces the rate, but you should compute differences robustly or keep runs well within the wrap interval.
- **Arduino `String`:** Can fragment heap on small MCUs. For large CSV dumps, prefer fixed-size buffers and `Serial.write()`.
- **Known cleanups (tracked):**
  - Replace any `pk1 = *(new waveformPkg);` pattern (leaks) with a stack object or placement new.
  - Make `dequeueToCSV()` print per-row (avoid cumulative `String` growth).

---
## License
Add your license (e.g., MIT) and include a `LICENSE` file at the repo root.

---
Questions or improvements? Open an issue with your board, core version, library versions, and a short log excerpt (metadata header + first ~20 waveform rows) so we can reproduce quickly.
