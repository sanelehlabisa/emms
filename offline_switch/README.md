# **Pseudo Code Summary**

## **SETUP:**
```
1. Initialize GPIO (relay, buzzer, LED, manual switch, keypad)
2. Initialize I2C communication
3. Initialize LCD (16x4)
4. Initialize RTC, set time if lost power
5. Load test schedules (optional, for testing)
6. Display "System Starting" message
```

## **LOOP:**
```
Every 1 second:
  - Read current time from RTC
  - Check if any schedule should activate
  - If relay ON, read current sensor and alert if no load
  - Update status LED
  - Auto-return to VIEW_STATUS after 30s inactivity
  - Update LCD display

Every 50ms:
  - Read keypad input
  - Handle key presses based on current mode

Continuously:
  - Check manual override switch
```

## **KEY FUNCTIONS:**

**handleKeypress(key):**
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

**checkSchedules():**
```
For each active schedule:
  If current time matches schedule time AND relay OFF:
    - Turn relay ON
    - Record start time and schedule index

If relay ON:
  If duration elapsed:
    - Turn relay OFF
```

**updateDisplay():**
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

**sortSchedules():**
```
Bubble sort schedules by time (earliest first)
