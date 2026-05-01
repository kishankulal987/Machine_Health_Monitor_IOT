/*
  ================================================================
  Industrial Machine Health Monitor — ESP32 Full Code v2
  ================================================================
  NEW in v2:
    - Relay AUTO-SHUTDOWN when status = CRITICAL
    - Physical RESTART BUTTON on GPIO 13
    - Webpage can send restart command via Firebase
    - Machine state (ON/OFF) synced to Firebase
  ================================================================
  Sensors:
    - DHT11   → Temperature & Humidity  → GPIO 4
    - ACS712  → Current (30A)           → GPIO 35
    - MPU9255 → Vibration (I2C)         → GPIO 21 (SDA), 22 (SCL)

  Outputs:
    - RELAY   → Machine power control   → GPIO 26  (Active LOW)
    - LED     → Alarm indicator         → GPIO 2   (Built-in)
    - BUTTON  → Physical restart        → GPIO 13  (Pull-up, press = GND)
  ================================================================
  LIBRARIES REQUIRED:
    1. Firebase ESP32 Client  by Mobizt
    2. DHT sensor library     by Adafruit
    3. Adafruit Unified Sensor by Adafruit
  ================================================================
*/

#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <DHT.h>
#include <Wire.h>
#include <math.h>

// ─────────────────────────────────────────────
// ★ EDIT THESE
// ─────────────────────────────────────────────
#define WIFI_SSID       "YOU WIFI NAME"
#define WIFI_PASSWORD   "PASSWORD OF WIFI"
#define API_KEY         "ENTER YOUR API KEY"
#define DATABASE_URL    "https://esp32-iot-15cfb-default-rtdb.firebaseio.com/"

// ─────────────────────────────────────────────
// Pin Definitions
// ─────────────────────────────────────────────
#define DHTPIN          4
#define DHTTYPE         DHT11
#define CURRENT_PIN     35
#define SDA_PIN         21
#define SCL_PIN         22
#define MPU_ADDR        0x68
#define LED_PIN         2       // Built-in LED
#define RELAY_PIN       26      // LOW = machine ON, HIGH = machine OFF
#define BUTTON_PIN      13      // Physical restart button (press = GND via pull-up)

// ─────────────────────────────────────────────
// ACS712 Calibration
// ─────────────────────────────────────────────
#define SENSITIVITY     0.066
#define ADC_SAMPLES     500

// ─────────────────────────────────────────────
// MPU Sensitivity
// ─────────────────────────────────────────────
#define ACCEL_SENS      16384.0
#define GYRO_SENS       131.0

// ─────────────────────────────────────────────
// Alert Thresholds
// ─────────────────────────────────────────────
#define TEMP_THRESHOLD      40.0
#define HUMIDITY_THRESHOLD  80.0
#define CURRENT_THRESHOLD   0.35
#define VIBRATION_THRESHOLD 0.3

// ─────────────────────────────────────────────
// Firebase objects
// ─────────────────────────────────────────────
FirebaseData   fbdo;
FirebaseData   fbdoCmd;   // separate object for stream listener
FirebaseAuth   auth;
FirebaseConfig config;

DHT dht(DHTPIN, DHTTYPE);

int16_t AccX, AccY, AccZ;
int16_t GyroX, GyroY, GyroZ;

// ─────────────────────────────────────────────
// State variables
// ─────────────────────────────────────────────
bool machineON        = true;
bool signupOK         = false;
unsigned long lastFirebaseSend = 0;
unsigned long lastBtnCheck     = 0;
unsigned long alarmBlinkTimer  = 0;
bool alarmBlinkState  = false;
float offsetVoltage = 0;

const unsigned long SEND_INTERVAL = 3000;

// ─────────────────────────────────────────────
// RELAY HELPERS
// ─────────────────────────────────────────────
void turnMachineON() {
  machineON = true;
  digitalWrite(RELAY_PIN, LOW);   // Active LOW → relay energized → machine ON
  digitalWrite(LED_PIN,   LOW);
  Serial.println("✅ Machine TURNED ON");
  Firebase.RTDB.setBool(&fbdo,   "/machine/control/machine_on", true);
  Firebase.RTDB.setString(&fbdo, "/machine/control/command",    "none");
}

void turnMachineOFF() {
  machineON = false;
  digitalWrite(RELAY_PIN, HIGH);  // De-energize relay → machine OFF
  Serial.println("🛑 Machine TURNED OFF — relay cut");
  Firebase.RTDB.setBool(&fbdo, "/machine/control/machine_on", false);
}

// ─────────────────────────────────────────────
// READ ACS712
// ─────────────────────────────────────────────
float readCurrent() {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) sum += analogRead(CURRENT_PIN);
  float voltage = (sum / (float)ADC_SAMPLES) * (3.3 / 4095.0);
  float current = (voltage - offsetVoltage) / SENSITIVITY;
  if (abs(current) < 0.05) current = 0.0;
  return current;
}

// ─────────────────────────────────────────────
// READ MPU9255
// ─────────────────────────────────────────────
float readVibration(float &aX, float &aY, float &aZ,
                    float &gX, float &gY, float &gZ) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (size_t)14, true);
  AccX  = Wire.read() << 8 | Wire.read();
  AccY  = Wire.read() << 8 | Wire.read();
  AccZ  = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();
  GyroX = Wire.read() << 8 | Wire.read();
  GyroY = Wire.read() << 8 | Wire.read();
  GyroZ = Wire.read() << 8 | Wire.read();

  aX = AccX / ACCEL_SENS; aY = AccY / ACCEL_SENS; aZ = AccZ / ACCEL_SENS;
  gX = GyroX / GYRO_SENS; gY = GyroY / GYRO_SENS; gZ = GyroZ / GYRO_SENS;

  float vib = sqrt(aX*aX + aY*aY + aZ*aZ) - 1.0;
  return (vib < 0) ? -vib : vib;
}

// ─────────────────────────────────────────────
// FIREBASE STREAM CALLBACK — listens for webpage "restart" command
// ─────────────────────────────────────────────
void streamCallback(FirebaseStream data) {
  String path = data.dataPath();
  String cmd  = "";

  if (data.dataType() == "string") {
    cmd = data.stringData();
  } else if (data.dataType() == "json") {
    FirebaseJson &json = data.jsonObject();
    FirebaseJsonData result;
    json.get(result, "command");
    if (result.success) cmd = result.stringValue;
  }

  cmd.trim();
  if (cmd.length() > 0 && cmd != "none") {
    Serial.print("📡 Firebase command: "); Serial.println(cmd);
  }

  if (cmd == "restart" && !machineON) {
    Serial.println("🌐 Webpage sent RESTART command");
    turnMachineON();
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) Serial.println("⚠️ Stream timeout — reconnecting...");
}

void calibrateACS() {// this function is to auto calibrate currrent offset
  Serial.println("Calibrating ACS712 (NO LOAD!)...");

  long sum = 0;

  for (int i = 0; i < 500; i++) {
    sum += analogRead(CURRENT_PIN);
    delay(2);
  }

  float avg = sum / 500.0;
  offsetVoltage = avg * (3.3 / 4095.0);

  Serial.print("New Offset Voltage: ");
  Serial.println(offsetVoltage);
}

// ─────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  Industrial Machine Health Monitor v2   ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  pinMode(LED_PIN,    OUTPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, LOW);  // Machine ON at boot
  digitalWrite(LED_PIN,   LOW);

  dht.begin();
  Serial.println("✅ DHT11 initialized");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  Wire.endTransmission(true);
  Serial.println("✅ MPU9255 initialized");
  calibrateACS();


  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("📶 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ WiFi: " + WiFi.localIP().toString());

  config.api_key         = API_KEY;
  config.database_url    = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
    Serial.println("✅ Firebase OK");
  } else {
    Serial.printf("❌ Firebase: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Set initial state
  Firebase.RTDB.setBool(&fbdo,   "/machine/control/machine_on", true);
  Firebase.RTDB.setString(&fbdo, "/machine/control/command",    "none");

  // Start listening for webpage commands
  if (!Firebase.RTDB.beginStream(&fbdoCmd, "/machine/control")) {
    Serial.println("⚠️ Stream failed: " + fbdoCmd.errorReason());
  } else {
    Firebase.RTDB.setStreamCallback(&fbdoCmd, streamCallback, streamTimeoutCallback);
    Serial.println("✅ Command stream listening");
  }

  Serial.println("\n🚀 Ready! GPIO13 button = physical restart\n");
  delay(1000);
}

// ─────────────────────────────────────────────
// MAIN LOOP
// ─────────────────────────────────────────────
void loop() {

  // ── Physical Button (debounced) ──
  if (millis() - lastBtnCheck > 50) {
    lastBtnCheck = millis();
    if (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if (digitalRead(BUTTON_PIN) == LOW) {
        if (!machineON) {
          Serial.println("🔘 Physical button → Restarting machine");
          turnMachineON();
        } else {
          Serial.println("🔘 Button pressed — machine already ON");
        }
        while (digitalRead(BUTTON_PIN) == LOW) delay(10); // wait for release
      }
    }
  }

  // ── If machine is OFF: blink LED slowly, skip sensors ──
  if (!machineON) {
    if (millis() - alarmBlinkTimer > 800) {
      alarmBlinkTimer = millis();
      alarmBlinkState = !alarmBlinkState;
      digitalWrite(LED_PIN, alarmBlinkState);
    }
    return;
  }

  // ── Read all sensors ──
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("❌ DHT11 error"); temperature = 0; humidity = 0;
  }

  float current = readCurrent();
  float aX, aY, aZ, gX, gY, gZ;
  float vibration = readVibration(aX, aY, aZ, gX, gY, gZ);

  // ── Alert flags ──
  bool tempAlert    = temperature > TEMP_THRESHOLD;
  bool humidAlert   = humidity    > HUMIDITY_THRESHOLD;
  bool currentAlert = current     > CURRENT_THRESHOLD;
  bool vibAlert     = vibration   > VIBRATION_THRESHOLD;
  bool anyAlert     = tempAlert || humidAlert || currentAlert || vibAlert;

  // ── Status ──
  String status = "NORMAL";
  if (anyAlert) status = "WARNING";

  bool isCritical =
    (temperature > TEMP_THRESHOLD    + 15) ||
    (current     > CURRENT_THRESHOLD)  ||
    (vibration   > VIBRATION_THRESHOLD + 0.3);
  if (isCritical) status = "CRITICAL";

  // ── CRITICAL → push to Firebase FIRST, then shut down ──
  if (isCritical) {
    Serial.println("🚨 CRITICAL! Sending to Firebase then shutting down...");
    FirebaseJson critJson;
    critJson.set("temperature",  temperature);
    critJson.set("humidity",     humidity);
    critJson.set("current",      current);
    critJson.set("vibration",    vibration);
    critJson.set("status",       "CRITICAL");
    critJson.set("uptime_sec",   (int)(millis() / 1000));
    Firebase.RTDB.setJSON(&fbdo, "/machine/sensor_data", &critJson);
    delay(500); // let Firebase receive it
    turnMachineOFF();
    return;
  }

  // ── Warning LED blink (fast) ──
  if (anyAlert) {
    if (millis() - alarmBlinkTimer > 150) {
      alarmBlinkTimer = millis();
      alarmBlinkState = !alarmBlinkState;
      digitalWrite(LED_PIN, alarmBlinkState);
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    alarmBlinkState = false;
  }

  // ── Serial Monitor ──
  Serial.println("══════════════════════════════════════");
  Serial.printf("🌡  Temp      : %.1f°C %s\n", temperature, tempAlert    ? "⚠️" : "✅");
  Serial.printf("💧 Humidity  : %.1f%%  %s\n", humidity,    humidAlert   ? "⚠️" : "✅");
  Serial.printf("⚡ Current   : %.2f A %s\n",  current,     currentAlert ? "⚠️" : "✅");
  Serial.printf("📳 Vibration : %.3f g %s\n",  vibration,   vibAlert     ? "⚠️" : "✅");
  Serial.printf("🔹 Accel X:%.2f Y:%.2f Z:%.2f\n", aX, aY, aZ);
  Serial.printf("🔹 Gyro  X:%.1f Y:%.1f Z:%.1f\n", gX, gY, gZ);
  Serial.printf("📊 Status    : %s\n", status.c_str());
  Serial.println("══════════════════════════════════════\n");

  // ── Send to Firebase every 3 seconds ──
  if (Firebase.ready() && signupOK &&
      millis() - lastFirebaseSend > SEND_INTERVAL) {
    lastFirebaseSend = millis();

    FirebaseJson json;
    json.set("temperature",        temperature);
    json.set("humidity",           humidity);
    json.set("current",            current);
    json.set("vibration",          vibration);
    json.set("accel/x",            aX);
    json.set("accel/y",            aY);
    json.set("accel/z",            aZ);
    json.set("gyro/x",             gX);
    json.set("gyro/y",             gY);
    json.set("gyro/z",             gZ);
    json.set("alerts/temperature", tempAlert);
    json.set("alerts/humidity",    humidAlert);
    json.set("alerts/current",     currentAlert);
    json.set("alerts/vibration",   vibAlert);
    json.set("status",             status);
    json.set("uptime_sec",         (int)(millis() / 1000));

    if (Firebase.RTDB.setJSON(&fbdo, "/machine/sensor_data", &json))
      Serial.println("🔥 Firebase sent ✅");
    else
      Serial.println("❌ Firebase: " + fbdo.errorReason());

    Firebase.RTDB.setBool(&fbdo, "/machine/control/machine_on", true);
  }

  delay(500);
}
