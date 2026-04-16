# Offline Switch

## Description

Offline Switch is a high current load electricaly appliance scheduler comprising RTC, Keypad and Relay.

---

## Getting Started

### Prerequisites
- Arduino CLI installed ([Installation Guide](https://arduino.github.io/arduino-cli/installation/))
- Git installed
- USB cable for Arduino board
- Hardware: Arduino UNO, 16x4 I2C LCD, DS3231 RTC, Relay Module, Current Sensor

### Project Setup

**1. Clone the repository**
```bash
git clone https://github.com/sanelehlabisa/emms.git
cd emms/offline_switch/hardware/arduino_uno
```

**2. Set up Arduino environment**
```bash
export ARDUINO_DATA_DIR=$(pwd)/.arduino
arduino-cli core update-index
```

**3. Install dependencies**
```bash
# Install board cores
while read core; do
  arduino-cli core install "$core"
done < cores.lock

# Install libraries
while read lib; do
  arduino-cli lib install "$lib"
done < libs.lock
```

**Cores file (`cores.lock`):**
```
arduino:avr@1.8.6
```

**Libraries file (`libs.lock`):**
```
RTClib@2.1.1
LiquidCrystal I2C@1.1.2
```

### Build and Upload

**Compile the sketch**
```bash
arduino-cli compile \
  --fqbn arduino:avr:uno \
  --build-path ./build \
  --output-dir ./dist \
  arduino_uno.ino
```

**Upload to board**
```bash
arduino-cli upload \
  -p /dev/ttyUSB0 \
  --fqbn arduino:avr:uno \
  arduino_uno.ino
```
*Note: Replace `/dev/ttyUSB0` with your port (Windows: `COM3`, macOS: `/dev/cu.usbserial`)*

**Find your board**
```bash
arduino-cli board list
```

### VS Code IntelliSense Setup

**Generate compile commands**
```bash
arduino-cli compile \
  --fqbn arduino:avr:uno \
  --build-path ./build \
  --only-compilation-database \
  arduino_uno.ino
```

### Development Workflow

**Install new library**
```bash
arduino-cli lib install "RTClib@2.1.1"
arduino-cli lib list | awk 'NR>1 {print $1"@"$2}' > libs.lock
```

**Update library**
```bash
arduino-cli lib upgrade RTClib
# Regenerate libs.lock after upgrade
```

**Clean build**
```bash
rm -rf build dist
arduino-cli compile --fqbn arduino:avr:uno --build-path ./build .
```

### Bootstrap Script (`scripts/bootstrap.sh`)
```bash
#!/bin/bash
export ARDUINO_DATA_DIR=$(pwd)/.arduino

echo "Installing board cores..."
while read core; do
  arduino-cli core install "$core"
done < cores.lock

echo "Installing libraries..."
while read lib; do
  arduino-cli lib install "$lib"
done < libs.lock

echo "Setup complete!"
```

**Usage on new machine:**
```bash
git clone <repo>
cd offline-switch
bash scripts/bootstrap.sh
```

---

## Working Principles

### Setup

1. Initialize GPIO (relay, buzzer, LED, manual switch, keypad)
2. Initialize Serial Communication (9600 baud)
3. Initialize I2C Communication
4. Initialize LCD (16x4 @ 0x20)
5. Initialize RTC, set time if lost power
6. Load test schedules (optional, for testing)
7. Display "System Starting" message

### Loop

**Every 1 second:**
- Read current time from RTC
- Check if any schedule should activate
- If relay ON, read current sensor and alert if no load
- Update status LED
- Auto-return to VIEW_STATUS after 30s inactivity
- Update LCD display

**Every 50ms:**
- Read keypad input
- Handle key presses based on current mode

**Continuously:**
- Check manual override switch

### Key Functions

**handleKeypress(key)**
```
If in VIEW_STATUS mode:
  - UP or DOWN → Enter EDIT_SCHEDULE mode
  - DOWN → Add new schedule if < 10 schedules

If in EDIT_SCHEDULE mode:
  - UP → Next schedule
  - DOWN → Previous schedule
  - LEFT/RIGHT → Move cursor (hour → minute → duration)
  - INC/DEC → Adjust selected field
  - DELETE → Remove current schedule
  - ENTER → Save and return to VIEW_STATUS
  - TOGGLE → Manual override
```

**checkSchedules()**
```
For each active schedule:
  If current time matches schedule time AND relay OFF:
    - Turn relay ON
    - Record start time and schedule index

If relay ON:
  If duration elapsed:
    - Turn relay OFF
```

**updateDisplay()**
```
Line 0: Current time (HH:MM:SS) + Relay status (ON/OFF)

If VIEW_STATUS:
  Line 1: Status (Manual Override / Off in X min / Standby)
  Line 2: Load current (X.X A)
  Line 3: Next event (Next ON/OFF HH:MM)

If EDIT_SCHEDULE:
  Line 1: Schedule number (Sch X/Y)
  Line 2: Time (HH:MM) with cursor indicator
  Line 3: Duration (XXX min) with cursor indicator
```

**sortSchedules()**
```
Bubble sort schedules by time (earliest first)
```
