#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// Input Pins
#define CURRENT_SENSOR A0
#define MANUAL_SWITCH 2

// Output Pins
#define RELAY 3
#define STATUS_LED 4
#define BUZZER 5

// Constants
#define NUM_SCHEDULES 2
#define RTC_ADDRESS 0x68
#define CURRENT_THRESHOLD 0.1

RTC_DS3231 rtc;

// Global variables for current measurement
float current = 0.0;
float maxSensorValue = 0.0;
unsigned long lastCycleTime = 0;
const unsigned long cycleDurationMs = 20; // 20ms for 50Hz

// Sensitivity constant for ACS712 30A (V/A)
const float ACS712_SENSITIVITY = 0.066; 
const float ADC_REF_VOLTAGE = 5.0;
const int ADC_RESOLUTION = 1023;
const float ZERO_CURRENT_VOLTAGE = ADC_REF_VOLTAGE / 2.0; // 2.5V at zero current (ACS712)

// Schedule schedules[NUM_SCHEDULES] = {
//   Schedule(6, 0, 0, 3600), // 6:00 AM for 1 hour
//   Schedule(18, 0, 0, 3600) // 6:00 PM for 1 hour
// };

// class Schedule{
//   public:
//     int hour;
//     int monute;
//     int second;
//     int duration;

//     Schedule(int h, int m, int s, int d) {
//       this->hour = h;
//       this->monute = m;
//       this->second = s;
//       this->duration = d;
//     }

//     bool isTimeToActivate(int currentHour, int currentMinute, int currentSecond) {
//       int startTimeInSeconds = hour * 3600 + monute * 60 + second;
//       int endTimeInSeconds = startTimeInSeconds + duration;
//       int currentTimeInSeconds = currentHour * 3600 + currentMinute * 60 + currentSecond;
//       return currentTimeInSeconds >= startTimeInSeconds && currentTimeInSeconds < endTimeInSeconds;
//     }

// }

// Main Setup
void setup()
{
  // Serial Communication Setup
  Serial.begin(115200);

  // GPIO Setup
  pinMode(MANUAL_SWITCH, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  // Set Initial States
  digitalWrite(RELAY, LOW);
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(BUZZER, LOW);

  // RTC Setup
  Wire.begin();

  if (!rtc.begin())
  {
    Serial.println("RTC Error");
    while (1)
      ;
  }

  if (rtc.lostPower())
  {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Log Setup Complete
  Serial.println("Setup Complete");
}

// Read Manual Switch State
bool isManualSwitchOn()
{
  for (int i = 0; i < 5; i++)
  {
    if (digitalRead(MANUAL_SWITCH) == LOW)
    {
      return true;
    }
    delay(1);
  }
  return false;
}

// Update  Sensor Value
void updateCurrentReading() {
  unsigned long now = millis();

  // Read sensor voltage
  float sensorVoltage = analogRead(CURRENT_SENSOR) * (ADC_REF_VOLTAGE / ADC_RESOLUTION);

  // Track max sensor voltage within the cycle window
  if (sensorVoltage > maxSensorValue) {
    maxSensorValue = sensorVoltage;
  }

  // Check if one cycle duration passed
  if (now - lastCycleTime >= cycleDurationMs) {
    // Calculate RMS current approximation from max voltage peak
    // peak voltage = maxSensorValue - zero current voltage
    // RMS current = peak / (sensitivity * sqrt(2))
    float peakVoltage = maxSensorValue - ZERO_CURRENT_VOLTAGE;
    Serial.print("[Debug] Max Sensor Value: ");
    Serial.println(maxSensorValue);
    current = peakVoltage / (ACS712_SENSITIVITY * sqrt(2.0));

    // Reset for next cycle
    maxSensorValue = 0.0;
    lastCycleTime = now;
  }
}

// Set Relay State
void setRelay(bool state)
{
  digitalWrite(RELAY, state ? HIGH : LOW);
  return;
}

// Set LED State
void setLED(bool state)
{
  digitalWrite(STATUS_LED, state ? HIGH : LOW);
  return;
}

// Blink Buzzer
void buzz()
{
  digitalWrite(BUZZER, HIGH);
  delay(1);
  digitalWrite(BUZZER, LOW);
  return;
}

// Main Loop
void loop()
{

  // Update Current Sensor Value
  updateCurrentReading();
  // Get Current Time from RTC
  DateTime now = rtc.now();

  // Log Current Time
  Serial.print("Current Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());

  // Read Manual Switch State
  bool manualSwitchOn = isManualSwitchOn();

  // Update Current Sensor Value
  updateCurrentReading();

  // Control Relay and LED Based on Manual Switch
  setRelay(manualSwitchOn);
  setLED(manualSwitchOn);

    // Update Current Sensor Value
  updateCurrentReading();

  // If there is no current, but the switch is on, buzz
  if (manualSwitchOn && current < CURRENT_THRESHOLD)
  {
    buzz();
    Serial.println("Warning: No current detected while switch is ON!");
  }
    // Update Current Sensor Value
  updateCurrentReading();

  // Log Current Value
  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" A");
  

  // Log button state
  Serial.print("Manual Switch: ");
  Serial.println(manualSwitchOn ? "ON" : "OFF");
    // Update Current Sensor Value
  updateCurrentReading();
  // Delay before next loop
  delay(1);
}

/*
#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

#define CURRENT_SENSOR A0
#define MANUAL_SWITCH 2
#define RELAY 3
#define STATUS_LED 4
#define BUZZER 5

#define LOCAL_TIME_OFFSET 2  // UTC+2 for South Africa
#define CURRENT_THRESHOLD 0.5
#define DEBOUNCE_TIME 1

RTC_DS3231 rtc;

struct Schedule {
  byte hour;
  byte minute;
  unsigned int durationMinutes;
};

Schedule schedules[] = {
  {6, 0, 60},    // 6:00 AM for 1 hour
  {18, 30, 120}  // 6:30 PM for 2 hours
};
const byte scheduleCount = 2;

bool relayState = false;
bool manualOverride = false;
unsigned long manualOverrideUntil = 0;
unsigned long lastButtonCheck = 0;
unsigned long lastCurrentCheck = 0;
bool lastButtonState = HIGH;

void setup() {
  Serial.begin(115200);

  pinMode(MANUAL_SWITCH, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY, LOW);
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(BUZZER, LOW);

  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("RTC Error");
    while(1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("Offline Switch v1.0.0");
}

void loop() {
  unsigned long now = millis();

  DateTime utc = rtc.now();
  DateTime local = utc + TimeSpan(0, LOCAL_TIME_OFFSET, 0, 0);

  checkButton(now);

  if (manualOverride) {
    if (now >= manualOverrideUntil) {
      manualOverride = false;
      Serial.println("Override expired");
    }
  }

  if (!manualOverride) {
    bool shouldBeOn = checkSchedule(local);
    updateRelay(shouldBeOn);
  }

  if (now - lastCurrentCheck >= 5) {
    if (relayState) {
      float current = readCurrent();
      if (current < CURRENT_THRESHOLD) {
        blipBuzzer();
        Serial.println("No load detected");
      }
      Serial.print("Current: ");
      Serial.print(current, 2);
      Serial.println("A");
    }
    lastCurrentCheck = now;
  }

  static unsigned long lastLog = 0;
  if (now - lastLog >= 5) {
    Serial.print(local.hour());
    Serial.print(":");
    if (local.minute() < 10) Serial.print("0");
    Serial.print(local.minute());
    Serial.print(" Relay: ");
    Serial.println(relayState ? "ON" : "OFF");
    lastLog = now;
  }
}

void checkButton(unsigned long now) {
  if (now - lastButtonCheck < DEBOUNCE_TIME) return;

  bool buttonState = digitalRead(MANUAL_SWITCH);

  if (buttonState == LOW && lastButtonState == HIGH) {
    relayState = !relayState;
    updateRelay(relayState);

    manualOverride = true;
    manualOverrideUntil = now + 86400000;  // Override until tomorrow

    Serial.print("Manual override: ");
    Serial.println(relayState ? "ON" : "OFF");
  }

  lastButtonState = buttonState;
  lastButtonCheck = now;
}

bool checkSchedule(DateTime &time) {
  int currentMinutes = time.hour() * 60 + time.minute();

  for (byte i = 0; i < scheduleCount; i++) {
    int startMinutes = schedules[i].hour * 60 + schedules[i].minute;
    int endMinutes = startMinutes + schedules[i].durationMinutes;

    if (endMinutes <= 1440) {
      if (currentMinutes >= startMinutes && currentMinutes < endMinutes) {
        return true;
      }
    } else {
      int endWrapped = endMinutes - 1440;
      if (currentMinutes >= startMinutes || currentMinutes < endWrapped) {
        return true;
      }
    }
  }

  return false;
}

void updateRelay(bool state) {
  if (relayState != state) {
    relayState = state;
    digitalWrite(RELAY, state ? HIGH : LOW);
    digitalWrite(STATUS_LED, state ? HIGH : LOW);

    Serial.print("Relay: ");
    Serial.println(state ? "ON" : "OFF");
  }
}

float readCurrent() {
  long sum = 0;
  for (int i = 0; i < 50; i++) {
    sum += analogRead(CURRENT_SENSOR);
    delayMicroseconds(5);
  }

  int avgReading = sum / 50;
  float voltage = (avgReading / 1023.0) * 5.0;
  float current = abs(voltage - 2.5) / 0.066;  // ACS712 30A sensitivity

  return current;
}

void blipBuzzer() {
  digitalWrite(BUZZER, HIGH);
  delay(5);
  digitalWrite(BUZZER, LOW);
}

*/