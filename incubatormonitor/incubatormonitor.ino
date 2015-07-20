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
- Add time acquisition on startup to use for log timestamp
- Add SD logging
- Add timestamp to SD log
- Add all analog inputs for recorder data acquisition

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
const int doorPin = 2;

const int upperCO2Pin = A0;
const int upperRHPin = A1;
const int upperTempPin = A2;
const int lowerCO2Pin = A3;
const int lowerRHPin = A4;
const int lowerTempPin = A5;

float upperCO2, upperRH, upperTempC, upperTempF;
float lowerCO2, lowerRH, lowerTempC, lowerTempF;

String currentDateTime;

SoftwareSerial gprsSerial(gprsRXPin, gprsTXPin);

void setup() {
  Serial.begin(19200);

  pinMode(gprsPowerPin, OUTPUT);
  pinMode(sdSlaveSelect, OUTPUT);

  gprsSerial.begin(19200);
  gprsStartup();
  if (debugMode) Serial.println(F("GPRS initiated."));
  if (!SD.begin(sdSlaveSelect)) programError(2);
  if (debugMode) Serial.println(F("SD card initiated."));

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
}

/*void loop() {
  boolean activeWarning = false;
  boolean doorState = false;
  boolean doorLastState = false;
  byte warningCode = 0;
  unsigned long timeoutStart = millis();
  while (!activeWarning) {
    getDateTime();
    sdLogData();
    for ( ; (millis() - timeoutStart) < doorTimeout; ) {
      if (digitalRead(doorPin) == true) timeoutStart = millis();
      delay(10);
    }
    warningCode = 1;
    activeWarning = true;
  }
  gprsSMSWarning(warningCode);
  activeWarning = false;
  // RESPOND TO ACTIVE WARNING APPROPRIATELY BASED ON WARNING CODE
  // ENTER EMERGENCY MONITORING AND LOGGING STATE UNTIL CONDITIONS NORMALIZE
  // RESET ACTIVE WARNING BOOLEAN WHEN CONDITIONS NORMALIZE
  // RETURN TO MAIN LOOP
}*/

void loop() {
  boolean activeWarning = false;
  boolean eventTimer = false;
  byte warningCode = 0;
  for (unsigned long monitorStart = millis(); (millis() - monitorStart) < logFrequency; ) {
    if (eventTimer == false && digitalRead(doorPin) == 1) {
      unsigned long doorOpenStart = millis();
      eventTimer = true;
    }
    else if (eventTimer == true && (millis() - doorOpenStart) > DOORTIMEOUT) {
      
    }
    /* Alerts & Monitoring */
    // Door
    // RJ-11 Warnings:
    // - CO2
    // - RH
    // - Temp
    
  }
  if (!activeWarning) sdLogData(false);
  else {
    // RESPOND TO WARNING
  }
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

// Send warning as SMS message. Error type defined as byte argument.
void gprsSMSWarning(byte warningCode) {
  String warningMessage;
  switch (warningCode) {
    case 0:
      break;
    case 1:
      warningMessage = "Alert: Bottom incubator door has been open for more than 2 minutes!";
      break;
    case 2:
      break;
    case 3:
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
}

// Assemble current date/time for log timestamp
String getDateTime() {
  currentDateTime = "";

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
  digitalWrite(gprsPowerPin, LOW);
  delay(100);
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

// Log data from incubator sensors. True = Log active warning / False = Regular sensor data logging
void sdLogData(boolean logWarning) {
  if (!logWarning) {
    
  }
  else {
    
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
