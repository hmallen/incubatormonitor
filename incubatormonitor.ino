/*
SMS Warning System for Cell-culture Incubator

- Arduino Uno
- SD Shield
- GPRS Shield w/ SIM card
- Magnetic reed switch

SMS sent to users on defined list in the event that incubator
conditions drift significantly out of range. This includes both
environmental conditions and extended period with door open.

TO DO:
- Add time acquisition on startup to use for log timestamp ****
- Add SD logging
- Add timestamp to SD log
- Add all analog inputs for recorder data acquisition

SMS Types (smsSent):
0 = None
1 = Door open duration passed threshold
2 = Upper incubator alarm
3 = Lower incubator alarm
*/

#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h>
#include <Time.h>

//// DEFINE PRESENCE OF INCUBATORS ////
boolean upperIncubator = false; // Set as true if upper incubator to be monitored
boolean lowerIncubator = true;  // Set as true if lower incubator to be monitored

//// DEFINE TIMEOUTS FOR ALERT AND LOG TRIGGERING ////
#define DOORTIMEOUT 120 // Number of seconds door must be open continuously before triggering SMS alert
#define LOGFREQUENCY 5  // Frequency in minutes that environmental data is logged to SD card
#define ALERTTRIGGERTIME 60 // Time in seconds that any alert must be persistently present to trigger SMS alert

const unsigned long logFrequency = LOGFREQUENCY * 60000;

//// ADD MOBILE NUMBERS FOR USERS THAT WILL RECEIVE SMS WARNING MESSAGES BELOW ////
char* userNumbers[] = {
  "2145635266", // Hunter Allen
  "xxxxxxxxxx", // Yousuf Ali
  "xxxxxxxxxx", // Gillian Bradley
  "xxxxxxxxxx", // Peggy
  "xxxxxxxxxx", // Marisha
  "xxxxxxxxxx"  // Diana
};
byte activeUsers = 0;

const unsigned long doorTimeout = DOORTIMEOUT * 1000;

const boolean debugMode = true;

const int gprsRXPin = 2;
const int gprsTXPin = 3;
const int gprsPowerPin = 4;
const int sdSlaveSelect = 10;

const int upperDoorPin = 5;
const int lowerDoorPin = 6;
const int upperAlarmPin = 7;
const int lowerAlarmPin = 8;

const int upperCO2Pin = A0;
const int upperRHPin = A1;
const int upperTempPin = A2;
const int lowerCO2Pin = A3;
const int lowerRHPin = A4;
const int lowerTempPin = A5;

boolean upperDoorState, upperAlarmState;
boolean lowerDoorState, lowerAlarmState;

float upperCO2, upperRH, upperTempC, upperTempF;
float lowerCO2, lowerRH, lowerTempC, lowerTempF;

boolean alertType[] = {false, false, false, false};  // 1st = Upper incubator door, 2nd = Lower incubator door, 3rd = Upper incubator alarm, 4th = Lower incubator alarm
boolean smsSent[] = {false, false, false, false};  // 1st = Upper incubator door, 2nd = Lower incubator door, 3rd = Upper incubator alarm, 4th = Lower incubator alarm

boolean upperDoorTrigger = false;
boolean lowerDoorTrigger = false;
boolean upperAlarmTrigger = false;
boolean lowerAlarmTrigger = false;
unsigned long upperDoorStart, upperAlarmStart;
unsigned long lowerDoorStart, lowerAlarmStart;

SoftwareSerial gprsSerial(gprsRXPin, gprsTXPin);

File logFile;

void setup() {
  Serial.begin(19200);

  pinMode(gprsPowerPin, OUTPUT);
  pinMode(sdSlaveSelect, OUTPUT);
  pinMode(upperDoorPin, INPUT);
  pinMode(lowerDoorPin, INPUT);
  pinMode(upperAlarmPin, INPUT);
  pinMode(lowerAlarmPin, INPUT);

  digitalWrite(gprsPowerPin, LOW);

  gprsSerial.begin(19200);
  gprsStartup();
  if (debugMode) Serial.println(F("GPRS initiated."));
  if (!SD.begin(sdSlaveSelect)) programError(2);
  if (debugMode) Serial.println(F("SD card initiated."));
  logFile = SD.open("incubatorlog.txt", FILE_WRITE);

  analogReference(INTERNAL);
  delay(1000);

  for (int x = 0; x < 10; x++) {
    if (upperIncubator) {
      int analogStabilization0 = analogRead(upperCO2Pin);
      delay(100);
      int analogStabilization1 = analogRead(upperRHPin);
      delay(100);
      int analogStabilization2 = analogRead(upperTempPin);
      delay(100);
    }
    if (lowerIncubator) {
      int analogStabilization3 = analogRead(lowerCO2Pin);
      delay(100);
      int analogStabilization4 = analogRead(lowerRHPin);
      delay(100);
      int analogStabilization5 = analogRead(lowerTempPin);
      delay(100);
    }
  }

  byte addressbookSize = (sizeof(userNumbers) / 2);
  for (int x = 0; x < addressbookSize; x++) {
    String tempStore = String(userNumbers[x]);
    char firstChar = tempStore.charAt(0);
    if (firstChar != 'x') activeUsers++;
  }
  if (logFile) {
    logFile.println(F("Time, Date, U-CO2, U-RH, U-Temp(F), U-Door, U-Alarm, L-CO2, L-RH, L-Temp(F), L-Door, L-Alarm"));
    logFile.close();
  }
  else {}// Generate header write error?
}

void loop() {
  for (unsigned long monitorStart = millis(); (millis() - monitorStart) < logFrequency; ) {
    doorCheck();
    alarmCheck();
    for (int x = 0; x < 4; x++) {
      if (alertType[x] == true) {
        responseCheck();
        break;
      }
    }
    delay(5000);
  }
  getRecorderData();
  sdLogData(0, false);
}

// Acquire and map data from external recorder outputs on back of incubators (Upper & Lower)
void getRecorderData() {
  if (upperIncubator) {
    int upperCO2Raw = analogRead(upperCO2Pin);
    upperCO2 = ((float)upperCO2Raw / 929.07) * 100.0;
    int upperRHRaw = analogRead(upperRHPin);
    upperRH = ((float)upperRHRaw / 929.07) * 100.0;
    int upperTempRaw = analogRead(upperTempPin);
    upperTempC = 45.0 - ((929.07 - (float)analogRead(upperTempPin)) * 0.1);
    upperTempF = (upperTempC * 1.8) + 32.0;
  }
  if (lowerIncubator) {
    int lowerCO2Raw = analogRead(lowerCO2Pin);
    lowerCO2 = ((float)lowerCO2Raw / 929.07) * 100.0;
    int lowerRHRaw = analogRead(lowerRHPin);
    lowerRH = ((float)lowerRHRaw / 929.07) * 100.0;
    int lowerTempRaw = analogRead(lowerTempPin);
    lowerTempC = 45.0 - ((929.07 - (float)analogRead(lowerTempPin)) * 0.1);
    lowerTempF = (lowerTempC * 1.8) + 32.0;
  }
}

// Check door state (open or closed) and set appropriate timers, booleans, etc.
void doorCheck() {
  if (upperIncubator) {
    upperDoorState = digitalRead(upperDoorPin);
    // Upper door
    if (upperDoorState) {
      if (upperDoorTrigger == false) {
        upperDoorStart = millis();
        upperDoorTrigger = true;
      }
      else if ((millis() - upperDoorStart) > ALERTTRIGGERTIME) alertType[0] = true;
    }
    else if (alertType[0] == true) {
      alertType[0] = false;
      upperDoorTrigger = false;
    }
  }
  if (lowerIncubator) {
    lowerDoorState = digitalRead(lowerDoorPin);
    // Lower door
    if (lowerDoorState) {
      if (lowerDoorTrigger == false) {
        lowerDoorStart = millis();
        lowerDoorTrigger = true;
      }
      else if ((millis() - lowerDoorStart) > ALERTTRIGGERTIME) alertType[1] = true;
    }
    else if (alertType[1] == true) {
      alertType[1] = false;
      lowerDoorTrigger = false;
    }
  }
}

// Check alarms
void alarmCheck() {
  /*if (digitalRead(upperAlarmPin) == 1) alertType[2] = true;
  else alertType[2] = false;
  if (digitalRead(lowerAlarmPin) == 1) alertType[3] = true;
  else alertType[3] = false;*/
  if (upperIncubator) {
    upperAlarmState = digitalRead(upperAlarmPin);
    // Upper alarm
    if (upperAlarmState) {
      if (upperAlarmTrigger == false) {
        upperAlarmStart = millis();
        upperAlarmTrigger = true;
      }
      else if ((millis() - upperAlarmStart) > ALERTTRIGGERTIME) alertType[2] = true;
    }
    else if (alertType[2] == true) {
      alertType[2] = false;
      upperAlarmTrigger = false;
    }
  }
  if (lowerIncubator) {
    lowerAlarmState = digitalRead(lowerAlarmPin);
    // Lower alarm
    if (lowerAlarmState) {
      if (lowerAlarmTrigger == false) {
        lowerAlarmStart = millis();
        lowerAlarmTrigger = true;
      }
      else if ((millis() - lowerAlarmStart) > ALERTTRIGGERTIME) alertType[3] = true;
    }
    else if (alertType[3] == true) {
      alertType[1] = false;
      lowerAlarmTrigger = false;
    }
  }
}

void responseCheck() {
  for (int x = 0; x < 4; x++) {
    if (alertType[x] == true && smsSent[x] == false) {
      gprsSMSAlert((x + 1));
      smsSent[x] = true;
    }
  }
}

// Log data from incubator sensors. True = Log active warning / False = Regular sensor data logging
void sdLogData(byte dataType, boolean logWarning) {
  String upperIncubatorData, lowerIncubatorData;
  String currentDateTime = getDateTime();
  if (logFile) {
    if (upperIncubator) {
      upperIncubatorData = currentDateTime + ',';
      upperIncubatorData += String(upperCO2) + ',' + String(upperRH) + ',' + String(upperTempF) + ',';
      upperIncubatorData += String(upperDoorState) + ',' + String(upperAlarmState);
    }
    if (lowerIncubator) {
      lowerIncubatorData = currentDateTime + ',';
      lowerIncubatorData += String(lowerCO2) + ',' + String(lowerRH) + ',' + String(lowerTempF) + ',';
      lowerIncubatorData += String(lowerDoorState) + ',' + String(lowerAlarmState);
    }
    if (!logWarning) {

    }
    else {
      // Log alert event or cessation of alert
    }
  }
  else {}  // Generate data log write error?
}

// Assemble current date/time for log timestamp
String getDateTime() {
  String currentDateTime = "";

  currentDateTime += formatDigits(hour());
  currentDateTime += ":";
  currentDateTime += formatDigits(minute());
  currentDateTime += ":";
  currentDateTime += formatDigits(second());
  currentDateTime += ",";
  currentDateTime += formatDigits(month());
  currentDateTime += "/";
  currentDateTime += formatDigits(day());
  currentDateTime += "/";
  currentDateTime += formatDigits(year());

  return currentDateTime;
}

// Send warning as SMS message. Error type defined as byte argument.
void gprsSMSAlert(byte alertCode) {
  String warningMessage;
  switch (alertCode) {
    case 0:
      break;
    case 1:
      warningMessage = "Alert: Upper incubator door has been open for more than DOORTIMEOUT seconds!";
      break;
    case 2:
      warningMessage = "Alert: Lower incubator door has been open for more than DOORTIMEOUT seconds!";
      break;
    case 3:
      warningMessage = "Alert: Upper incubator alarm is active!";
      break;
    case 4:
      warningMessage = "Alert: Lower incubator alarm is active!";
      break;
    default:
      break;
  }
  gprsSerial.print(F("AT+CMGS=\"+1"));
  delay(100);
  for (int i = 0; i < activeUsers; i++) {
    gprsSerial.print(userNumbers[i]);
    delay(100);
    gprsSerial.println(F("\""));
    delay(100);
    gprsSerial.println(warningMessage);
    delay(100);
    gprsSerial.println((char)26);
    delay(100);
    gprsSerial.flush();
    if (!gprsSerial.available()) {
      while (!gprsSerial.available()) {
        delay(10);
      }
    }
    gprsSerialFlush(false);
    if (!gprsSerial.available()) {
      while (!gprsSerial.available()) {
        delay(10);
      }
    }
    gprsSerialFlush(false);
  }
  smsSent[0] = true;  // Door open time passed threshold
}

// Format data during date/time assembly and return to main function
String formatDigits(int digits) {
  String formattedDigits = "";
  if (digits < 10) {
    formattedDigits += '0';
    formattedDigits += digits;
  }
  else formattedDigits += digits;

  return formattedDigits;
}

// Perform software power-on of GPRS. Send AT commands to set appropriate paramaters for normal operation.
void gprsStartup() {
  digitalWrite(gprsPowerPin, HIGH);
  delay(500);
  digitalWrite(gprsPowerPin, LOW);
  delay(100);

  if (!gprsSerial.available()) {
    unsigned long startTime = millis();
    while (!gprsSerial.available()) {
      if ((millis() - startTime) > 10000) break;
      delay(10);
    }
  }
  if (debugMode) gprsSerialFlush(true);
  else gprsSerialFlush(false);

  // Configure GPRS output for proper parsing
  gprsSerial.println(F("ATE0"));
  delay(100);
  gprsSerial.println(F("ATQ1"));
  delay(100);
  gprsSerial.println(F("ATV0"));
  delay(100);
  gprsSerial.println(F("AT+CMGF=1"));
  delay(100);

  if (debugMode) gprsSerialFlush(true);
  else gprsSerialFlush(false);

  gprsSerial.println(F("AT+CCLK?"));
  delay(100);
  if (!gprsSerial.available()) {
    while (!gprsSerial.available()) {
      delay(10);
    }
  }
  String gprsDateTimeRaw, gprsDateTime;
  while (gprsSerial.available()) {
    char c = gprsSerial.read();
    gprsDateTimeRaw += c;
    delay(10);
  }
  for (int x = (gprsDateTimeRaw.indexOf('"') + 1); x < gprsDateTimeRaw.lastIndexOf('"'); x++) {
    char c = gprsDateTimeRaw.charAt(x);
    gprsDateTime += c;
  }

  String monthRaw, dayRaw, yearRaw, hourRaw, minuteRaw, secondRaw, gmtOffsetRaw;
  byte charPos = 0;
  // Parse all date and time values from GPRS
  for (int x = charPos; x < 2; x++) {
    char c = gprsDateTime.charAt(x);
    yearRaw += c;
    charPos++;
  }
  int year = yearRaw.toInt();
  for (int x = (charPos + 1); x < 5; x++) {
    char c = gprsDateTime.charAt(x);
    monthRaw += c;
    charPos++;
  }
  int month = monthRaw.toInt();
  for (int x = (charPos + 1); x < 8; x++) {
    char c = gprsDateTime.charAt(x);
    dayRaw += c;
    charPos++;
  }
  int day = dayRaw.toInt();
  for (int x = (charPos + 1); x < 11; x++) {
    char c = gprsDateTime.charAt(x);
    hourRaw += c;
    charPos++;
  }
  int hour = hourRaw.toInt();
  for (int x = (charPos + 1); x < 14; x++) {
    char c = gprsDateTime.charAt(x);
    minuteRaw += c;
    charPos++;
  }
  int minute = minuteRaw.toInt();
  for (int x = (charPos + 1); x < 17; x++) {
    char c = gprsDateTime.charAt(x);
    secondRaw += c;
    charPos++;
  }
  int second = secondRaw.toInt();
  for (int x = (charPos + 1); x < 20; x++) {
    char c = gprsDateTime.charAt(x);
    gmtOffsetRaw += c;
  }
  int gmtOffset = gmtOffsetRaw.toInt() / 4;

  setTime(hour , minute, second, day, month, year);
}

// Flush GPRS Serial output. True = Output to serial monitor / False = Flush without output
void gprsSerialFlush(boolean serialPrint) {
  while (gprsSerial.available()) {
    char c = gprsSerial.read();
    if (serialPrint) Serial.print(c);
    delay(10);
    if (!gprsSerial.available()) delay(2500);
  }
}

// Trigger program halt in the event of an unrecoverable error. Error type defined as byte argument.
void programError(byte errorCode) {
  switch (errorCode) {
    case 0:
      break;
    case 1: // GPRS serial initialization error
      Serial.println(F("GPRS failed to initialize."));
      break;
    case 2: // SD SPI initialization error
      Serial.println(F("SD card failed to initialize."));
      break;
    case 3:
      break;
    default:
      break;
  }
  while (true) {
    Serial.println(F("Please reset device."));
    delay(60000);
  }
}
