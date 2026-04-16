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

rtc DS3231;

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
void setup() {
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
  Serial.begin(9600);
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1) {
      Serial.println("...");
      delay(10);
    }
  }

  // Set RTC time if not already set
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Log Setup Complete
  Serial.println("Setup Complete");
}

// Read Manual Switch State
bool isManualSwitchOn() {
  for(int i = 0; i < 5; i++) {
    if (digitalRead(MANUAL_SWITCH) == LOW) {
      return true;
    }
    delay(1);
  }
  return false;
}

// Read  Sensor Value
float getCurrent() {
  float voltage = analogRead(CURRENT_SENSOR) * (5.0 / 1023.0);
  return round((5.0 / 2.0 - voltage) * 100.0) / 100.0;
}

// Set Relay State
void setRelay(bool state) {
  digitalWrite(RELAY, state ? HIGH : LOW);
  return;
}

// Set LED State
void setLED(bool state) {
  digitalWrite(STATUS_LED, state ? HIGH : LOW);
  return;
}

// Blink Buzzer
void buzz() {
  digitalWrite(BUZZER, HIGH);
  delay(1);
  digitalWrite(BUZZER, LOW);
  return;
}

// Main Loop
void loop() {
  // Get Current Time from RTC
  DateTime now = rtc.getc();

  // Log Current Time
  Serial.print("Current Time: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());

  // Read Manual Switch State
  bool manualSwitchOn = isManualSwitchOn();

  // Read Current Sensor Value
  float current = getCurrent();

  // Control Relay and LED Based on Manual Switch
  setRelay(manualSwitchOn);
  setLED(manualSwitchOn);

  // If there is no current, but the switch is on, buzz
  if (manualSwitchOn && current < CURRENT_THRESHOLD) {
    buzz();
    Serial.println("Warning: No current detected while switch is ON!");
  }

  // Log Current Value
  Serial.print("Current: ");
  Serial.print(current);
  Serial.println(" A");

  // Log button state
  Serial.print("Manual Switch: ");
  Serial.println(manualSwitchOn ? "ON" : "OFF");

  // Delay before next loop
  delay(10);
}