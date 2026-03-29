#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>

#define MANUAL_SWITCH 2
#define RELAY 3
#define BUZZER 4
#define STATUS_LED 5
#define ROW1 8
#define ROW2 9
#define ROW3 10
#define COL1 11
#define COL2 12
#define COL3 13
#define CURRENT_SENSOR A0

const byte ROWS = 3;
const byte COLS = 3;
byte rowPins[ROWS] = {ROW1, ROW2, ROW3};
byte colPins[COLS] = {COL1, COL2, COL3};
char keys[ROWS][COLS] = {
  {'I', 'U', 'X'},  // Inc, Up, Delete
  {'L', 'E', 'R'},  // Left, Enter, Right
  {'D', 'N', 'T'}   // Dec, dowN, Toggle
};

LiquidCrystal_I2C lcd(0x20, 16, 4);
RTC_DS3231 rtc;

struct Schedule {
  byte hour;
  byte minute;
  unsigned int durationMinutes;
  bool active;
};

Schedule schedules[10];
byte scheduleCount = 0;

DateTime currentTime;
bool relayState = false;
bool manualOverride = false;
unsigned long relayOnTime = 0;
int activeScheduleIndex = -1;

enum UIMode { VIEW_STATUS, EDIT_SCHEDULE };
UIMode currentMode = VIEW_STATUS;
int selectedSchedule = 0;
int editField = 0;  // 0=hour, 1=minute, 2=duration
unsigned long lastInteraction = 0;

const float ACS712_SENSITIVITY = 0.066;

void setup() {
  Serial.begin(9600);
  
  pinMode(MANUAL_SWITCH, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  
  digitalWrite(RELAY, LOW);
  digitalWrite(BUZZER, LOW);
  digitalWrite(STATUS_LED, LOW);
  
  for (byte i = 0; i < ROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], HIGH);
  }
  for (byte i = 0; i < COLS; i++) {
    pinMode(colPins[i], INPUT_PULLUP);
  }

  Wire.begin();
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("System Starting");
  
  if (!rtc.begin()) {
    lcd.setCursor(0, 1);
    lcd.print("RTC Error!");
    while(1);
  }
  
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  currentTime = rtc.now();
  
  // Test schedules (comment out for production)
  /*
  schedules[0] = {6, 0, 30, true};
  schedules[1] = {18, 30, 60, true};
  scheduleCount = 2;
  sortSchedules();
  */
  
  delay(1000);
  lcd.clear();
}

void loop() {
  static unsigned long lastSecond = 0;
  static unsigned long lastKeyCheck = 0;
  
  unsigned long now = millis();
  
  // Update every second
  if (now - lastSecond >= 1000) {
    currentTime = rtc.now();
    checkSchedules();
    
    if (relayState) {
      float current = readCurrent();
      if (current < 0.5) blipBuzzer(1);
    }
    
    digitalWrite(STATUS_LED, relayState);
    
    // Auto-return to status view after 30s inactivity
    if (currentMode == EDIT_SCHEDULE && (now - lastInteraction > 30000)) {
      currentMode = VIEW_STATUS;
    }
    
    updateDisplay();
    lastSecond = now;
  }
  
  // Check keypad every 50ms
  if (now - lastKeyCheck >= 50) {
    char key = readKeypad();
    if (key != '\0') {
      lastInteraction = now;
      handleKeypress(key);
    }
    lastKeyCheck = now;
  }
  
  // Manual override switch
  if (digitalRead(MANUAL_SWITCH) == LOW) {
    delay(30);
    if (digitalRead(MANUAL_SWITCH) == LOW) {
      manualOverride = !manualOverride;
      if (manualOverride) turnOffRelay();
      while(digitalRead(MANUAL_SWITCH) == LOW);
    }
  }
}

char readKeypad() {
  for (byte r = 0; r < ROWS; r++) {
    digitalWrite(rowPins[r], LOW);
    delayMicroseconds(5);
    
    for (byte c = 0; c < COLS; c++) {
      if (digitalRead(colPins[c]) == LOW) {
        char key = keys[r][c];
        digitalWrite(rowPins[r], HIGH);
        delay(150);
        return key;
      }
    }
    digitalWrite(rowPins[r], HIGH);
  }
  return '\0';
}

float readCurrent() {
  long sum = 0;
  for (int i = 0; i < 20; i++) {
    sum += analogRead(CURRENT_SENSOR);
  }
  int avgReading = sum / 20;
  float voltage = (avgReading / 1023.0) * 5.0;
  return abs((voltage - 2.5) / ACS712_SENSITIVITY);
}

void handleKeypress(char key) {
  
  if (key == 'T') {
    manualOverride = !manualOverride;
    if (manualOverride) turnOffRelay();
    return;
  }
  
  if (currentMode == VIEW_STATUS) {
    
    if (key == 'U' || key == 'N') {
      currentMode = EDIT_SCHEDULE;
      if (key == 'N' && scheduleCount < 10) {
        schedules[scheduleCount].hour = currentTime.hour();
        schedules[scheduleCount].minute = currentTime.minute();
        schedules[scheduleCount].durationMinutes = 60;
        schedules[scheduleCount].active = true;
        selectedSchedule = scheduleCount;
        scheduleCount++;
      } else {
        selectedSchedule = 0;
      }
      editField = 0;
    }
    
  } else {
    
    if (key == 'U') {
      selectedSchedule = (selectedSchedule + 1) % scheduleCount;
      editField = 0;
    }
    
    else if (key == 'N') {
      if (scheduleCount > 0) {
        selectedSchedule = (selectedSchedule - 1 + scheduleCount) % scheduleCount;
        editField = 0;
      }
    }
    
    else if (key == 'R' || key == 'L') {
      editField = (editField + 1) % 3;
    }
    
    else if (key == 'I') {
      if (editField == 0) {
        schedules[selectedSchedule].hour = (schedules[selectedSchedule].hour + 1) % 24;
      } else if (editField == 1) {
        schedules[selectedSchedule].minute = (schedules[selectedSchedule].minute + 1) % 60;
      } else {
        schedules[selectedSchedule].durationMinutes = min(schedules[selectedSchedule].durationMinutes + 5, 1440);
      }
    }
    
    else if (key == 'D') {
      if (editField == 0) {
        schedules[selectedSchedule].hour = (schedules[selectedSchedule].hour + 23) % 24;
      } else if (editField == 1) {
        schedules[selectedSchedule].minute = (schedules[selectedSchedule].minute + 59) % 60;
      } else {
        schedules[selectedSchedule].durationMinutes = max(schedules[selectedSchedule].durationMinutes - 5, 5);
      }
    }
    
    else if (key == 'X') {
      if (scheduleCount > 0) {
        for (int i = selectedSchedule; i < scheduleCount - 1; i++) {
          schedules[i] = schedules[i + 1];
        }
        scheduleCount--;
        if (selectedSchedule >= scheduleCount && scheduleCount > 0) {
          selectedSchedule = scheduleCount - 1;
        }
      }
    }
    
    else if (key == 'E') {
      sortSchedules();
      currentMode = VIEW_STATUS;
    }
  }
}

void checkSchedules() {
  for (byte i = 0; i < scheduleCount; i++) {
    if (!schedules[i].active) continue;
    
    if (currentTime.hour() == schedules[i].hour && 
        currentTime.minute() == schedules[i].minute &&
        currentTime.second() < 2 &&
        !relayState &&
        !manualOverride) {
      turnOnRelay(i);
      return;
    }
  }
  
  if (relayState && activeScheduleIndex >= 0) {
    unsigned long elapsed = (millis() - relayOnTime) / 60000;
    if (elapsed >= schedules[activeScheduleIndex].durationMinutes) {
      turnOffRelay();
    }
  }
}

void turnOnRelay(int scheduleIndex) {
  relayState = true;
  activeScheduleIndex = scheduleIndex;
  relayOnTime = millis();
  digitalWrite(RELAY, HIGH);
}

void turnOffRelay() {
  relayState = false;
  activeScheduleIndex = -1;
  digitalWrite(RELAY, LOW);
}

void blipBuzzer(byte count) {
  for (byte i = 0; i < count; i++) {
    digitalWrite(BUZZER, HIGH);
    delay(80);
    digitalWrite(BUZZER, LOW);
    delay(80);
  }
}

void sortSchedules() {
  for (int i = 0; i < scheduleCount - 1; i++) {
    for (int j = i + 1; j < scheduleCount; j++) {
      int t1 = schedules[i].hour * 60 + schedules[i].minute;
      int t2 = schedules[j].hour * 60 + schedules[j].minute;
      if (t2 < t1) {
        Schedule temp = schedules[i];
        schedules[i] = schedules[j];
        schedules[j] = temp;
      }
    }
  }
}

void updateDisplay() {
  lcd.clear();
  
  // Line 0: Current time
  lcd.setCursor(0, 0);
  if (currentTime.hour() < 10) lcd.print("0");
  lcd.print(currentTime.hour());
  lcd.print(":");
  if (currentTime.minute() < 10) lcd.print("0");
  lcd.print(currentTime.minute());
  lcd.print(":");
  if (currentTime.second() < 10) lcd.print("0");
  lcd.print(currentTime.second());
  
  lcd.print(" ");
  lcd.print(relayState ? "ON " : "OFF");
  
  if (currentMode == VIEW_STATUS) {
    
    // Line 1: Current status
    lcd.setCursor(0, 1);
    if (manualOverride) {
      lcd.print("Manual Override");
    } else if (relayState && activeScheduleIndex >= 0) {
      unsigned long elapsed = (millis() - relayOnTime) / 60000;
      int remaining = schedules[activeScheduleIndex].durationMinutes - elapsed;
      lcd.print("Off in ");
      lcd.print(remaining);
      lcd.print(" min");
    } else {
      lcd.print("Standby");
    }
    
    // Line 2: Current sensor
    lcd.setCursor(0, 2);
    lcd.print("Load: ");
    if (relayState) {
      lcd.print(readCurrent(), 1);
      lcd.print("A");
    } else {
      lcd.print("0.0A");
    }
    
    // Line 3: Next event
    lcd.setCursor(0, 3);
    int next = getNextSchedule();
    if (next >= 0) {
      if (relayState) {
        lcd.print("Next OFF ");
      } else {
        lcd.print("Next ON  ");
      }
      if (schedules[next].hour < 10) lcd.print("0");
      lcd.print(schedules[next].hour);
      lcd.print(":");
      if (schedules[next].minute < 10) lcd.print("0");
      lcd.print(schedules[next].minute);
    } else {
      lcd.print("No schedules");
    }
    
  } else {
    
    // Line 1: Schedule number
    lcd.setCursor(0, 1);
    lcd.print("Sch ");
    lcd.print(selectedSchedule + 1);
    lcd.print("/");
    lcd.print(scheduleCount);
    
    if (scheduleCount == 0) {
      lcd.setCursor(0, 2);
      lcd.print("Press DOWN to add");
      return;
    }
    
    // Line 2: Time
    lcd.setCursor(0, 2);
    lcd.print("Time: ");
    if (schedules[selectedSchedule].hour < 10) lcd.print("0");
    lcd.print(schedules[selectedSchedule].hour);
    lcd.print(":");
    if (schedules[selectedSchedule].minute < 10) lcd.print("0");
    lcd.print(schedules[selectedSchedule].minute);
    
    // Line 3: Duration
    lcd.setCursor(0, 3);
    lcd.print("Dur:  ");
    lcd.print(schedules[selectedSchedule].durationMinutes);
    lcd.print(" min");
    
    // Cursor indicator
    if (editField == 0) {
      lcd.setCursor(6, 2);
      lcd.print("^^");
    } else if (editField == 1) {
      lcd.setCursor(9, 2);
      lcd.print("^^");
    } else {
      lcd.setCursor(6, 3);
      lcd.print("^^^^");
    }
  }
}

int getNextSchedule() {
  if (scheduleCount == 0) return -1;
  
  int nowMinutes = currentTime.hour() * 60 + currentTime.minute();
  int bestIndex = -1;
  int minDiff = 1441;
  
  for (int i = 0; i < scheduleCount; i++) {
    int schedMinutes = schedules[i].hour * 60 + schedules[i].minute;
    int diff = schedMinutes - nowMinutes;
    if (diff < 0) diff += 1440;
    
    if (diff < minDiff) {
      minDiff = diff;
      bestIndex = i;
    }
  }
  
  return bestIndex;
}
