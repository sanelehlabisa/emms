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

### **SETUP:**
```
1. Initialize serial communication (115200 baud)
2. Configure GPIO pins:
   - MANUAL_SWITCH as input with pullup
   - RELAY, STATUS_LED, BUZZER as outputs
3. Set all outputs to LOW (off state)
4. Initialize I2C communication
5. Initialize RTC:
   - If RTC not found, halt with error
   - If RTC lost power, set to compile time
6. Read initial time from RTC
7. Initialize timing variable (lastMillis)
8. Print "Setup Complete"
```

---

### **LOOP:**
```
1. Get current millisecond timestamp (via getTime())

2. Update currentTime variable:
   - Calculate elapsed time since last update
   - If >= 1 second has passed:
     - Increment currentTime by elapsed seconds
     - Reset lastMillis
     - [On real hardware: read fresh time from RTC]

3. Read current sensor:
   - Sample ADC voltage from sensor
   - Track peak voltage over 20ms window
   - Calculate RMS current from peak
   - Reset peak tracker every cycle

4. Check manual button:
   - Apply 50ms debounce delay
   - If button pressed (LOW):
     - Toggle relay state
     - Set overrideActive flag
     - Log override action

5. Get schedule state (should relay be ON?):
   - Convert current time to minutes
   - Check each schedule:
     - Calculate start/end times in minutes
     - Handle midnight wrapping (e.g., 23:00 + 120min = 01:00)
     - Return true if current time within any schedule

6. Handle schedule state changes:
   - If scheduleState changed since last loop:
     - Clear overrideActive flag
     - Log "override expired"
     - Update prevScheduleState

7. Apply relay control:
   - If NOT overridden:
     - Set relay to match schedule state

8. Check for load issues:
   - If relay ON but current < threshold:
     - Buzz every 1 second
     - Log "No load detected"

9. Log status (every 10ms):
   - Print: Time, Relay State, Current
   - Calculate minutes until next event:
     - If currently IN schedule: time until OFF
     - If currently OUT: time until next ON
   - Print countdown
```

---

### **KEY FUNCTIONS:**

**checkSchedule():**
```
Convert current time to total minutes (hour*60 + minute)
For each schedule:
  Calculate start time in minutes
  Calculate end time (start + duration)
  
  If schedule does NOT cross midnight:
    Return TRUE if current time between start and end
  
  If schedule crosses midnight (end > 1440):
    Wrap end time (subtract 1440)
    Return TRUE if current time after start OR before wrapped end

Return FALSE if no schedule active
```

**getMinutesUntilNextEvent():**
```
If currently IN schedule:
  Find which schedule is active
  Calculate end time
  Handle midnight wrapping
  Return (end - current) in minutes

If currently OUT of schedule:
  Find nearest upcoming start time
  Handle day wrapping (tomorrow's schedule)
  Return difference in minutes
```

**updateRelay(state):**
```
If new state differs from current relay state:
  Update relayState variable
  Set RELAY pin HIGH/LOW
  Set STATUS_LED pin HIGH/LOW
```

---

**getTime() - abstraction layer:**
```
Real hardware: return millis()
Proteus simulation: return micros()/1000
Future RTC-based: return rtc-derived milliseconds
```
