/*
SMS Warning System for Cell-culture Incubator

- Arduino Uno
- SD Shield
- GPRS Shield w/ SIM card
- Magnetic reed switch

SMS sent to users on defined list in the event that incubator
conditions drift significantly out of range. This includes both
environmental conditions and extended period with door open.
*/

#include <SoftwareSerial.h>
#include <SD.h>
#include <SPI.h>

//// ADD MOBILE NUMBERS FOR USERS THAT WILL RECEIVE SMS WARNING MESSAGES BELOW ////
/*
 * To try:
 * Array of arrays
 * Ex.
 * numberList[] = {
 * NUM1
 * NUM2
 * NUM3
 * Etc.
 * }
 */
const char user0[11] = "2145635266";  // Hunter Allen
const char user1[11] = "xxxxxxxxxx";  // Yousuf Ali
const char user2[11] = "xxxxxxxxxx";  // Gillian Bradley
const char user3[11] = "xxxxxxxxxx";  // Peggy
const char user4[11] = "xxxxxxxxxx";  // Marisha
const char user5[11] = "xxxxxxxxxx";  // Diana

//// DEFINE TIMEOUTS FOR ALERT TRIGGERING ////
#define DOORTIMEOUT 120 // Number of seconds door must be open continuously before triggering SMS alert

const unsigned long doorTimeout = DOORTIMEOUT * 1000;

const boolean debugMode = true;

const int gprsRXPin = 7;
const int gprsTXPin = 8;
const int gprsPowerPin = 9;
const int sdSlaveSelect = 10;
const int doorPin = 2;

SoftwareSerial gprsSerial(gprsRXPin, gprsTXPin);

void setup() {
  Serial.begin(9600);
  pinMode(gprsPowerPin, OUTPUT);
  pinMode(sdSlaveSelect, OUTPUT);

  gprsSerial.begin(19200);
  gprsStartup();
  if (debugMode) Serial.println(F("GPRS initiated."));
  if (!SD.begin(sdSlaveSelect)) programError(2);
  if (debugMode) Serial.println(F("SD card initiated."));

  byte userCount = 0;
  if (user0[0] != 'x') userCount++;
  if (user1[0] != 'x') userCount++;
  if (user2[0] != 'x') userCount++;
  if (user3[0] != 'x') userCount++;
  if (user4[0] != 'x') userCount++;
  if (user5[0] != 'x') userCount++;
}

void loop() {
  boolean activeWarning = false;
  boolean doorState = false;
  boolean doorLastState = false;
  byte warningCode = 0;
  unsigned long timeoutStart = millis();
  while (!activeWarning) {
    // MONITOR CONDITIONS
    // IF CONDITIONS DRIFT PAST THRESHOLD, START TIMER AND MONITOR CLOSELY
    // IF CONDITION TIMER ELAPSES BEFORE CONDITIONS NORMALIZE, TRIGGER ACTIVE WARNING BOOLEAN AND SET WARNING CODE
    for ( ; (millis() - timeoutStart) < doorTimeout; ) {
      if (digitalRead(doorPin) == true) doorState = true;
      else doorState = false;

      if (doorLastState != doorState) timeoutStart = millis();
      doorLastState = doorState;
    }
    warningCode = 1;
    activeWarning = true;
    gprsSMSWarning(warningCode);
  }
  // RESPOND TO ACTIVE WARNING APPROPRIATELY BASED ON WARNING CODE
  // ENTER EMERGENCY MONITORING AND LOGGING STATE UNTIL CONDITIONS NORMALIZE
  // RESET ACTIVE WARNING BOOLEAN WHEN CONDITIONS NORMALIZE
  // RETURN TO MAIN LOOP
}

// Send warning as SMS message. Error type defined as byte argument.
void gprsSMSWarning(byte warningCode) {
  boolean smsReady = false;
  switch (warningCode) {
    case 0:
      break;
    case 1:
      gprsSerial.println(F("AT+CMGF=1"));
      delay(100);
      gprsSerial.print(F("AT+CMGS=\"+1"));
      delay(100);
      //// TEMPORARY METHOD ////
      for (int i = 0; i < 10; i++) {
        gprsSerial.print(user0[i]);
        delay(100);
      }
      /////////////////////////
      //gprsSerial.println(F("\""));
      //delay(100);
      //gprsSerial.println(F("Buzzer activated."));
      //delay(100);
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
      break;
    case 2:
      break;
    case 3:
      break;
    default:
      break;
  }
  if (smsReady) {
    /*  INSERT APPROPRIATE AT COMMANDS TO SEND SMS  */
    //gprsSerial.print();
    //etc.
  }
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
    while (!gprsSerial.available()) {
      delay(10);
    }
  }
  if (debugMode) gprsSerialFlush(false);
  else gprsSerialFlush(true);

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
