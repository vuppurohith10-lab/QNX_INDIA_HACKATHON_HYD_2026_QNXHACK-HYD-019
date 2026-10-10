/*
 * QNX SDV - MILITARY VEHICLE MONITORING SYSTEM
 *
 * HC-05 Bluetooth -> ESP32 -> ST7735 TFT

 */

#include "BluetoothSerial.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// ------------ BLUETOOTH ------------

BluetoothSerial SerialBT;

uint8_t hc05Address[6] = {
  0x20, 0x25, 0x08, 0x00, 0xF9, 0x1E
};

bool btConnected = false;

unsigned long lastBTAttempt = 0;
const unsigned long BT_RETRY_MS = 5000;

// ------------ TFT DISPLAY ------------

#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4

Adafruit_ST7735 tft(TFT_CS, TFT_DC, TFT_RST);

// ------------ VEHICLE DATA ------------

String vehicleID = "A1206";
String mode = "NORMAL";
String state = "WAITING";
String message = "Waiting for vehicle data";
String rxBuffer = "";
String lastRawData = "";

unsigned long lastReceived = 0;
unsigned long lastTFTUpdate = 0;

const unsigned long DATA_TIMEOUT_MS = 10000;

// ------------ MESSAGE FIELD PARSER ------------

String getField(const String& line, const String& key) {
  String token = key + ":";
  int start = line.indexOf(token);

  if (start < 0) {
    return "";
  }

  start += token.length();

  int end = line.indexOf(';', start);

  if (end < 0) {
    end = line.length();
  }

  String value = line.substring(start, end);
  value.trim();

  return value;
}

// ------------ STATUS MESSAGE ------------

String getStatusMessage(const String& currentState) {
  if (currentState == "TRAFFIC") {
    return "APPROACHING TARGET";
  }

  if (currentState == "OBSTACLE") {
    return "TARGET DAMAGED";
  }

  if (currentState == "CLEAR") {
    return "NO TARGET DETECTED";
  }

  if (currentState == "EMERGENCY") {
    return "EMERGENCY";
  }

  return "Waiting for vehicle data";
}

// ------------ DRAW TFT ------------

void drawTFT() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.setTextSize(1);

  // Header
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(4, 5);
  tft.println("QNX SDV");

  tft.drawFastHLine(0, 17, 160, ST77XX_BLUE);

  // Vehicle ID
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 24);
  tft.print("VEHICLE ID: ");
  tft.println(vehicleID);

  // Operating mode
  tft.setCursor(4, 41);
  tft.print("MODE: ");
  tft.println(mode);

  // State color
  uint16_t stateColor = ST77XX_WHITE;

  if (state == "CLEAR") {
    stateColor = ST77XX_GREEN;
  } else if (state == "TRAFFIC") {
    stateColor = ST77XX_YELLOW;
  } else if (state == "OBSTACLE" ||
             state == "EMERGENCY") {
    stateColor = ST77XX_RED;
  }

  tft.setTextColor(stateColor);
  tft.setCursor(4, 58);
  tft.print("STATE: ");

  if (state == "TRAFFIC") {
    tft.println("APPROACHING");
  } else if (state == "OBSTACLE") {
    tft.println("DAMAGED");
  } else if (state == "CLEAR") {
    tft.println("NO TARGET");
  } else {
    tft.println(state);
  }

  // Bluetooth connection
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 77);
  tft.print("BLUETOOTH: ");
  tft.println(btConnected ? "CONNECTED" : "OFFLINE");

  // Vehicle status message
  tft.setCursor(4, 94);
  tft.println("VEHICLE MESSAGE:");

  tft.setTextColor(stateColor);
  tft.setCursor(4, 107);
  tft.println(message);
}

// ------------ CONNECT HC-05 ------------

bool tryConnect() {
  Serial.println("[BT] Connecting to HC-05...");

  bool result = SerialBT.connect(hc05Address);
  btConnected = result;

  Serial.println(
    result ? "[BT] Connected" : "[BT] Connection failed"
  );

  drawTFT();

  return result;
}

// ------------ PROCESS RECEIVED DATA ------------

void processMessage(String line) {
  line.trim();

  if (line.length() == 0) {
    return;
  }

  Serial.print("[BT] Received: ");
  Serial.println(line);

  lastRawData = line;

  String newID = getField(line, "ID");
  String newMode = getField(line, "MODE");
  String newState = getField(line, "STATE");
  String newMessage = getField(line, "MSG");

  if (newID.length() > 0) {
    vehicleID = newID;
  }

  if (newMode.length() > 0) {
    mode = newMode;
  }

  if (newState == "CLEAR" ||
      newState == "TRAFFIC" ||
      newState == "OBSTACLE" ||
      newState == "EMERGENCY") {

    state = newState;

    // Use the mapped status message unless QNX
    // explicitly supplies a MSG field.
    message = newMessage.length() > 0
                ? newMessage
                : getStatusMessage(state);

  } else if (newMessage.length() > 0) {
    message = newMessage;
  } else {
    Serial.println("[BT] Unknown state; keeping previous status");
    return;
  }

  lastReceived = millis();

  Serial.print("[STATUS] ");
  Serial.println(message);

  drawTFT();
}

// ------------ SETUP ------------

void setup() {
  Serial.begin(115200);
  delay(500);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(4, 5);
  tft.println("QNX SDV");

  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(4, 25);
  tft.println("Starting...");

  if (SerialBT.begin("QNX_SDV_ESP32", true)) {
    Serial.println("[BT] Bluetooth master ready");
    SerialBT.setPin("1234", 4);
  } else {
    Serial.println("[BT] Initialization failed");
  }

  drawTFT();

  // Attempt first connection immediately.
  lastBTAttempt = millis() - BT_RETRY_MS;
}

// ------------ MAIN LOOP ------------

void loop() {
  // Reconnect when Bluetooth is disconnected.
  if (!btConnected &&
      millis() - lastBTAttempt >= BT_RETRY_MS) {

    lastBTAttempt = millis();
    tryConnect();
  }

  // Receive newline-terminated messages.
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

  // Detect Bluetooth disconnection.
  if (btConnected && !SerialBT.connected()) {
    btConnected = false;

    Serial.println("[BT] Disconnected");

    drawTFT();
  }

  // Mark data stale after 10 seconds.
  if (lastReceived > 0 &&
      millis() - lastReceived > DATA_TIMEOUT_MS &&
      state != "WAITING") {

    state = "WAITING";
    message = "No recent vehicle data";

    drawTFT();
  }

  // Refresh display periodically.
  if (millis() - lastTFTUpdate >= 1000) {
    lastTFTUpdate = millis();
    drawTFT();
  }

  delay(2);
}
