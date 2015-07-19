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

//// ADD MOBILE NUMBERS FOR USERS THAT WILL RECEIVE SMS WARNING MESSAGES BELOW ////
char* userNumbers[] = {
  "2145635266", // Hunter Allen
  "xxxxxxxxxx", // Yousuf Ali
  "xxxxxxxxxx", // Gillian Bradley
  "xxxxxxxxxx", // Peggy
  "xxxxxxxxxx", // Marisha
  "xxxxxxxxxx"  // Diana
}
byte activeUsers = 0;

//// DEFINE TIMEOUTS FOR ALERT TRIGGERING ////
#define DOORTIMEOUT 120 // Number of seconds door must be open continuously before triggering SMS alert

const unsigned long doorTimeout = DOORTIMEOUT * 1000;

const boolean debugMode = true;

const int gprsRXPin = 2;
const int gprsTXPin = 3;
const int gprsPowerPin = 4;
const int sdSlaveSelect = 10;
const int doorPin = 2;

String currentDateTime;

SoftwareSerial gprsSerial(gprsRXPin, gprsTXPin);

void setup() {
  Serial.begin(19200);
  pinMode(gprsPowerPin, OUTPUT);
  pinMode(sdSlaveSelect, OUTPUT);

  //gprsSerial.begin(19200);
  //gprsStartup();
  //if (debugMode) Serial.println(F("GPRS initiated."));
  //if (!SD.begin(sdSlaveSelect)) programError(2);
  //if (debugMode) Serial.println(F("SD card initiated."));

  byte addressbookSize = (sizeof(userNumbers) / 2);
  for (int x = 0; x < addressbookSize; x++) {
    String tempStore = String(userNumbers[x]);
    char firstChar = tempStore.charAt(0);
    if (firstChar != 'x') activeUsers++;
  }
}

void loop() {
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
void getDateTime() {
  currentDateTime = "";

  currentDateTime += month();
  currentDateTime += "/";
  currentDateTime += day();
  currentDateTime += "/";
  currentDateTime += year();
  currentDateTime += ",";
  currentDateTime += formatDigits(hour());
  currentDateTime += ":";
  currentDateTime += formatDigits(minute());
  currentDateTime += ":";
  currentDateTime += formatDigits(second());
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

void sdLogData() {
  // LOG DATA HERE
}

// Trigger program halt in the event of an unrecoverable error. Error type defined as byte argument.
void programError(byte errorCode) {
  switch (errorCode) {
    case 0:
      break;
    // GPRS serial initialization error
    case 1:
      Serial.println(F("GPRS failed to initialize."));
      break;
    // SD SPI initialization error
    case 2:
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
