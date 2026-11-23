/*
 * ============================================================================
 * QNX GLOBAL E-HACK 2026 - SDV MILITARY VEHICLE CONTROL ROOM
 *
 * ESP32 + ST7735 1.8" TFT (160x128 SPI) + HC-05 Bluetooth Master
 *
 * Telemetry Received from QNX SDV:
 * "ID:A1206;MODE:SDV_AUTONOMOUS;STATE:TRAFFIC;TOF:32.4;SIGN:speed_limit_30;PWM:30,30;MSG:..."
 * ============================================================================
 */

#include "BluetoothSerial.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ------------ BLUETOOTH CONFIGURATION ------------
BluetoothSerial SerialBT;

// Default HC-05 MAC Address (update to your module's MAC)
uint8_t hc05Address[6] = {
  0x20, 0x25, 0x08, 0x00, 0xF9, 0x1E
};

bool btConnected = false;
unsigned long lastBTAttempt = 0;
const unsigned long BT_RETRY_MS = 4000;

// ------------ ST7735 TFT DISPLAY PINOUT (SPI) ------------
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ------------ TELEMETRY & ADAS STATE ------------
String vehicleID   = "A1206";
String mode        = "SDV_AUTO";
String state       = "WAITING";
String tofDistance = "--";
String signName    = "NONE";
String motorPWM    = "0,0";
String message     = "Waiting for QNX vehicle data...";
String rxBuffer    = "";
String lastRawData = "";

float tofCm = 100.0;
unsigned long lastReceived = 0;
unsigned long lastTFTUpdate = 0;
const unsigned long DATA_TIMEOUT_MS = 10000;

// ------------ FIELD PARSER ------------
String getField(const String& line, const String& key) {
  String token = key + ":";
  int start = line.indexOf(token);
  if (start < 0) return "";
  start += token.length();
  int end = line.indexOf(';', start);
  if (end < 0) end = line.length();
  String value = line.substring(start, end);
  value.trim();
  return value;
}

// ------------ COLOR SCHEME ------------
#define COLOR_BG        ST77XX_BLACK
#define COLOR_HEADER    ST77XX_CYAN
#define COLOR_BORDER    0x2104  // Dark Grey
#define COLOR_CLEAR     ST77XX_GREEN
#define COLOR_CAUTION   ST77XX_YELLOW
#define COLOR_DANGER    ST77XX_RED
#define COLOR_SIGN_BOX  0x001F  // Deep Blue
#define COLOR_SIGN_TXT  ST77XX_WHITE

// ------------ TFT RENDER FUNCTION ------------
void drawTFT() {
  tft.fillScreen(COLOR_BG);
  tft.setTextWrap(false);

  // Top Title Bar
  tft.fillRect(0, 0, 160, 16, 0x0841);
  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(4, 4);
  tft.print("QNX SDV :: BATT-TANK");

  // BT Icon / Status
  tft.setCursor(118, 4);
  if (btConnected) {
    tft.setTextColor(ST77XX_GREEN);
    tft.print("[V2V ON]");
  } else {
    tft.setTextColor(ST77XX_RED);
    tft.print("[OFFLINE]");
  }

  // Row 1: Vehicle ID & Operating Mode
  tft.drawFastHLine(0, 17, 160, COLOR_BORDER);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 21);
  tft.print("ID: ");
  tft.setTextColor(ST77XX_CYAN);
  tft.print(vehicleID);

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(85, 21);
  tft.print("MODE: ");
  tft.setTextColor(ST77XX_GREEN);
  tft.println(mode);

  // Row 2: ToF Distance Sensor Bar
  tft.setCursor(4, 34);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("ToF DIST: ");

  uint16_t distColor = COLOR_CLEAR;
  if (tofCm <= 20.0) distColor = COLOR_DANGER;
  else if (tofCm <= 40.0) distColor = COLOR_CAUTION;

  tft.setTextColor(distColor);
  tft.print(tofDistance);
  tft.println(" cm");

  // Visual Distance Gauge (4 to 156 px)
  tft.drawRect(4, 46, 152, 6, 0x52AA);
  int barWidth = (int)((tofCm / 100.0) * 150);
  if (barWidth > 150) barWidth = 150;
  if (barWidth < 0) barWidth = 0;
  tft.fillRect(5, 47, barWidth, 4, distColor);

  // Row 3: V800 Motor Status
  tft.setCursor(4, 56);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("V800 PWM: ");
  tft.setTextColor(ST77XX_MAGENTA);
  tft.print("L/R: ");
  tft.print(motorPWM);
  tft.print("%");

  // Row 4: Traffic Sign ADAS Badge
  tft.drawFastHLine(0, 68, 160, COLOR_BORDER);
  tft.fillRect(4, 72, 152, 20, 0x18E3);
  tft.drawRect(4, 72, 152, 20, 0x4A69);

  tft.setCursor(8, 78);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("SIGN: ");

  uint16_t signColor = ST77XX_GREEN;
  String signDisplay = signName;

  if (signName == "stop") {
    signColor = ST77XX_RED;
    signDisplay = "STOP [HALT]";
  } else if (signName == "no_entry") {
    signColor = ST77XX_RED;
    signDisplay = "NO ENTRY [ALERT]";
  } else if (signName == "speed_limit_30") {
    signColor = ST77XX_CYAN;
    signDisplay = "SPEED 30 KM/H";
  } else if (signName == "school_ahead") {
    signColor = ST77XX_YELLOW;
    signDisplay = "SCHOOL ZONE [SLOW]";
  } else if (signName == "pedestrian_crossing") {
    signColor = ST77XX_YELLOW;
    signDisplay = "PEDESTRIAN CROSS";
  } else if (signName == "slippery_road") {
    signColor = ST77XX_CYAN;
    signDisplay = "SLIPPERY ROAD";
  } else if (signName == "right_hand_curve") {
    signColor = ST77XX_MAGENTA;
    signDisplay = "RIGHT CURVE [BEND]";
  } else if (signName == "give_way") {
    signColor = ST77XX_ORANGE;
    signDisplay = "GIVE WAY [YIELD]";
  } else if (signName == "narrow_road_ahead") {
    signColor = ST77XX_YELLOW;
    signDisplay = "NARROW CORRIDOR";
  } else if (signName == "no_parking") {
    signColor = ST77XX_GREEN;
    signDisplay = "NO PARKING [CRUISE]";
  } else if (signName == "none" || signName == "NONE") {
    signColor = 0x7BEF;
    signDisplay = "ROADWAY CLEAR";
  }

  tft.setTextColor(signColor);
  tft.print(signDisplay);

  // Row 5: Operational State Banner
  tft.setCursor(4, 96);
  tft.setTextColor(ST77XX_WHITE);
  tft.print("STATE: ");

  uint16_t stateColor = COLOR_CLEAR;
  if (state == "OBSTACLE" || state == "EMERGENCY") stateColor = COLOR_DANGER;
  else if (state == "TRAFFIC" || state == "SLOW") stateColor = COLOR_CAUTION;

  tft.setTextColor(stateColor);
  tft.println(state);

  // Row 6: Vehicle Mission Message
  tft.setCursor(4, 108);
  tft.setTextColor(0xAD55);
  tft.print("MSG: ");
  tft.setTextColor(stateColor);
  tft.println(message.substring(0, 24));
}

// ------------ CONNECT HC-05 BLUETOOTH ------------
bool tryConnect() {
  Serial.println("[BT] Attempting connection to HC-05...");
  bool result = SerialBT.connect(hc05Address);
  btConnected = result;
  Serial.println(result ? "[BT] Connected to QNX SDV!" : "[BT] Connection retry...");
  drawTFT();
  return result;
}

// ------------ PARSE RECEIVED QNX TELEMETRY ------------
void processMessage(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("[RX Telemetry] ");
  Serial.println(line);
  lastRawData = line;

  String newID    = getField(line, "ID");
  String newMode  = getField(line, "MODE");
  String newState = getField(line, "STATE");
  String newTof   = getField(line, "TOF");
  String newSign  = getField(line, "SIGN");
  String newPwm   = getField(line, "PWM");
  String newMsg   = getField(line, "MSG");

  if (newID.length() > 0)    vehicleID = newID;
  if (newMode.length() > 0)  mode = newMode;
  if (newState.length() > 0) state = newState;
  if (newTof.length() > 0) {
    tofDistance = newTof;
    tofCm = newTof.toFloat();
  }
  if (newSign.length() > 0)  signName = newSign;
  if (newPwm.length() > 0)   motorPWM = newPwm;
  if (newMsg.length() > 0)   message = newMsg;

  lastReceived = millis();
  drawTFT();
}

// ------------ SETUP ------------
void setup() {
  Serial.begin(115200);
  delay(200);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);

  tft.setTextColor(COLOR_HEADER);
  tft.setTextSize(1);
  tft.setCursor(4, 10);
  tft.println("QNX GLOBAL E-HACK 2026");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 30);
  tft.println("SDV CONTROL ROOM");
  tft.setCursor(4, 50);
  tft.setTextColor(ST77XX_YELLOW);
  tft.println("Initializing Bluetooth...");

  if (SerialBT.begin("QNX_SDV_CONTROL", true)) {
    Serial.println("[BT] ESP32 Bluetooth Master Initialized");
    SerialBT.setPin("1234", 4);
  } else {
    Serial.println("[BT] Bluetooth Init Failed");
  }

  delay(500);
  lastBTAttempt = millis() - BT_RETRY_MS;
  drawTFT();
}

// ------------ MAIN LOOP ------------
void loop() {
  // Bluetooth Reconnect Watchdog
  if (!btConnected && (millis() - lastBTAttempt >= BT_RETRY_MS)) {
    lastBTAttempt = millis();
    tryConnect();
  }

  // Receive Telemetry Characters
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();
    if (c == '\n') {
      processMessage(rxBuffer);
      rxBuffer = "";
    } else if (c != '\r') {
      if (rxBuffer.length() < 300) {
        rxBuffer += c;
      } else {
        rxBuffer = "";
      }
    }
  }

  // Detect Bluetooth Link Loss
  if (btConnected && !SerialBT.connected()) {
    btConnected = false;
    Serial.println("[BT] Link lost.");
    drawTFT();
  }

  // Data Timeout (Stale data watchdog)
  if (lastReceived > 0 && (millis() - lastReceived > DATA_TIMEOUT_MS) && state != "OFFLINE") {
    state = "OFFLINE";
    message = "No telemetry received for 10s";
    drawTFT();
  }

  // Periodic Refresh
  if (millis() - lastTFTUpdate >= 1000) {
    lastTFTUpdate = millis();
    drawTFT();
  }

  delay(5);
}
