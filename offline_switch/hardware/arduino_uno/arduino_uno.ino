#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

#define CURRENT_SENSOR A0
#define MANUAL_SWITCH 2
#define RELAY 3
#define STATUS_LED 4
#define BUZZER 5

#define CURRENT_THRESHOLD 0.1
#define DEBOUNCE_TIME 50

bool prevScheduleState = false;
bool overrideActive = false;

RTC_DS3231 rtc;

struct Schedule
{
  byte hour;
  byte minute;
  unsigned int durationMinutes;
};

Schedule schedules[] = {
    {6, 0, 60},
    {16, 0, 120}};
const byte scheduleCount = 2;

float current = 0.0;
float maxSensorValue = 0.0;
unsigned long lastCycleTime = 0;
const unsigned long cycleDurationMs = 20;

const float ACS712_SENSITIVITY = 0.066;
const float ADC_REF_VOLTAGE = 5.0;
const int ADC_RESOLUTION = 1023;
const float ZERO_CURRENT_VOLTAGE = ADC_REF_VOLTAGE / 2.0;

bool relayState = false;
unsigned long lastButtonCheck = 0;
bool lastButtonState = HIGH;

DateTime currentTime;
unsigned long lastMillis = 0;

void setup()
{
  Serial.begin(115200);

  pinMode(MANUAL_SWITCH, INPUT_PULLUP);
  pinMode(RELAY, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(RELAY, LOW);
  digitalWrite(STATUS_LED, LOW);
  digitalWrite(BUZZER, LOW);

  Wire.begin();

  if (!rtc.begin())
  {
    Serial.println("RTC Error");
    while (1);
  }

  if (rtc.lostPower())
  {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  currentTime = rtc.now();
  lastMillis = micros(); // millis(); For simulation

  Serial.println("Setup Complete");
}

void updateCurrentReading()
{
  unsigned long now = micros(); // millis(); // For simulation

  float sensorVoltage = analogRead(CURRENT_SENSOR) * (ADC_REF_VOLTAGE / ADC_RESOLUTION);

  if (sensorVoltage > maxSensorValue)
  {
    maxSensorValue = sensorVoltage;
  }

  if (now - lastCycleTime >= cycleDurationMs)
  {
    float peakVoltage = maxSensorValue - ZERO_CURRENT_VOLTAGE;
    current = peakVoltage / (ACS712_SENSITIVITY * sqrt(2.0));
    maxSensorValue = 0.0;
    lastCycleTime = now;
  }
}

void checkButton(unsigned long now)
{
  if (now - lastButtonCheck < DEBOUNCE_TIME)
    return;

  bool buttonState = digitalRead(MANUAL_SWITCH);

  if (buttonState == LOW && lastButtonState == HIGH)
  {
    relayState = !relayState;
    digitalWrite(RELAY, relayState ? HIGH : LOW);
    digitalWrite(STATUS_LED, relayState ? HIGH : LOW);

    overrideActive = true;

    Serial.print("Manual Override: ");
    Serial.println(relayState ? "ON" : "OFF");
  }

  lastButtonState = buttonState;
  lastButtonCheck = now;
}

bool checkSchedule()
{
  int currentMinutes = currentTime.hour() * 60 + currentTime.minute();

  for (byte i = 0; i < scheduleCount; i++)
  {
    int startMinutes = schedules[i].hour * 60 + schedules[i].minute;
    int endMinutes = startMinutes + schedules[i].durationMinutes;

    if (endMinutes <= 1440)
    {
      if (currentMinutes >= startMinutes && currentMinutes < endMinutes)
      {
        return true;
      }
    }
    else
    {
      int endWrapped = endMinutes - 1440;
      if (currentMinutes >= startMinutes || currentMinutes < endWrapped)
      {
        return true;
      }
    }
  }
  return false;
}

void updateRelay(bool state)
{
  if (relayState != state)
  {
    relayState = state;
    digitalWrite(RELAY, state ? HIGH : LOW);
    digitalWrite(STATUS_LED, state ? HIGH : LOW);
  }
}

void buzzIfNeeded()
{
  static unsigned long lastBuzz = 0;
  unsigned long now = micros(); // millis(); For simulation

  if (relayState && current < CURRENT_THRESHOLD)
  {
    if (now - lastBuzz >= 10)
    {
      digitalWrite(BUZZER, HIGH);
      delay(1);
      digitalWrite(BUZZER, LOW);
      lastBuzz = now;
      Serial.println("No load detected");
    }
  }
}

int getMinutesUntilNextEvent()
{
  int currentMinutes = currentTime.hour() * 60 + currentTime.minute();
  bool currentlyInSchedule = checkSchedule();

  if (currentlyInSchedule)
  {
    for (byte i = 0; i < scheduleCount; i++)
    {
      int startMinutes = schedules[i].hour * 60 + schedules[i].minute;
      int endMinutes = startMinutes + schedules[i].durationMinutes;

      if (endMinutes <= 1440)
      {
        if (currentMinutes >= startMinutes && currentMinutes < endMinutes)
        {
          return endMinutes - currentMinutes;
        }
      }
      else
      {
        int endWrapped = endMinutes - 1440;
        if (currentMinutes >= startMinutes)
        {
          return (1440 - currentMinutes) + endWrapped;
        }
        else if (currentMinutes < endWrapped)
        {
          return endWrapped - currentMinutes;
        }
      }
    }
  }
  else
  {
    int minDiff = 1441;
    for (byte i = 0; i < scheduleCount; i++)
    {
      int startMinutes = schedules[i].hour * 60 + schedules[i].minute;
      int diff = startMinutes - currentMinutes;
      if (diff < 0)
        diff += 1440;
      if (diff < minDiff)
        minDiff = diff;
    }
    return minDiff;
  }

  return 0;
}

void loop()
{
  unsigned long now = micros(); // millis(); For simulation

  unsigned long elapsed = now - lastMillis;
  if (elapsed >= 1000)
  {
    currentTime = currentTime + TimeSpan(elapsed / 1000);
    lastMillis = now;
  }

  updateCurrentReading();
  checkButton(now);

  bool scheduleState = checkSchedule();

  if (scheduleState != prevScheduleState)
  {
    overrideActive = false;
    Serial.println("Schedule changed - override expired");
    prevScheduleState = scheduleState;
  }

  if (!overrideActive)
  {
    updateRelay(scheduleState);
  }

  buzzIfNeeded();
  static unsigned long lastLog = 0;
  if (now - lastLog >= 10)
  {
    Serial.print("Time: ");
    Serial.print(currentTime.hour());
    Serial.print(":");
    if (currentTime.minute() < 10)
      Serial.print("0");
    Serial.print(currentTime.minute());
    Serial.print(" | Relay: ");
    Serial.print(relayState ? "ON" : "OFF");
    Serial.print(" | Current: ");
    Serial.print(current, 2);
    Serial.print("A | ");

    int minutesLeft = getMinutesUntilNextEvent();
    if (checkSchedule())
    {
      Serial.print("OFF in ");
    }
    else
    {
      Serial.print("ON in ");
    }
    Serial.print(minutesLeft);
    Serial.println(" min");

    lastLog = now;
  }
}
