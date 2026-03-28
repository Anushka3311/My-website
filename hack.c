#include <Wire.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h> 

// --- Pins & Thresholds ---
#define MPU_ADDR 0x68
#define BUZZER_PIN 8      
#define LED_PIN 13
#define RESET_BUTTON_PIN 7   // Button to stop the alarm
#define PANIC_BUTTON_PIN 6   // New dedicated Panic/SOS button

LiquidCrystal_I2C lcd(0x27, 16, 2); 

const float IMPACT_THRESHOLD = 4.00; 
const float ROTATION_THRESHOLD = 250.0;
const unsigned long TRACKING_INTERVAL = 10000; 
const long BEEP_INTERVAL = 500;       
const long ALARM_DURATION = 15000;    // Increased for better visibility

static const int RXPin = 4, TXPin = 3;
SoftwareSerial ss(RXPin, TXPin);
TinyGPSPlus gps;

unsigned long lastLogTime = 0;
unsigned long lastBeepTime = 0;
unsigned long alertStartTime = 0;    
bool isAlertActive = false;
bool buzzerState = false;

void setup() {
  Serial.begin(9600);
  ss.begin(9600);
  Wire.begin();
  
  lcd.begin();
  lcd.backlight();
  displayMedicalInfo(); 

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); 
  Wire.write(0);    
  Wire.endTransmission(true);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Set up both buttons with internal pullups
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP); 
  pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
  
  Serial.println(F("SYSTEM START: Monitoring Sensors, Panic, & Reset..."));
}

void loop() {
  while (ss.available() > 0) {
    gps.encode(ss.read());
  }

  // --- 1. DEDICATED PANIC BUTTON LOGIC ---
  if (digitalRead(PANIC_BUTTON_PIN) == HIGH&& !isAlertActive) {
      Serial.println(F("PANIC BUTTON PRESSED!"));
      triggerEmergency(0.0); // 0.0 force indicates manual trigger
      isAlertActive = true;
      alertStartTime = millis();
      delay(300); // Debounce
  }

  // --- 2. DEDICATED RESET BUTTON LOGIC ---
  if (digitalRead(RESET_BUTTON_PIN) == HIGH && isAlertActive) {
      stopAlert("Manual System Reset");
      delay(300); // Debounce
  }

  // --- 3. SENSOR MONITORING ---
  if (!isAlertActive) {
    checkAccident();
  } else {
    handleActiveAlert(); 
  }

  // Periodic GPS Logging
  if (millis() - lastLogTime >= TRACKING_INTERVAL) {
    logLocation();
    lastLogTime = millis();
  }
}

void displayMedicalInfo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Blood Grp: AB+");
  lcd.setCursor(0, 1);
  lcd.print("Disease:HIGH BP");
}

void checkAccident() {
  int16_t raX, raY, raZ, rgX, rgY, rgZ;
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);

  raX = (Wire.read() << 8) | Wire.read();
  raY = (Wire.read() << 8) | Wire.read();
  raZ = (Wire.read() << 8) | Wire.read();
  Wire.read(); Wire.read(); 
  rgX = (Wire.read() << 8) | Wire.read();
  rgY = (Wire.read() << 8) | Wire.read();
  rgZ = (Wire.read() << 8) | Wire.read();

  float ax = (float)raX / 16384.0;
  float ay = (float)raY / 16384.0;
  float az = (float)raZ / 16384.0;
  float gx = (float)rgX / 131.0;
  float gy = (float)rgY / 131.0;
  float gz = (float)rgZ / 131.0;

  float total_accel = sqrt(ax*ax + ay*ay + az*az);
  float total_gyro = sqrt(gx*gx + gy*gy + gz*gz);

  if (total_accel > IMPACT_THRESHOLD || total_gyro > ROTATION_THRESHOLD) {
    isAlertActive = true;
    alertStartTime = millis(); 
    triggerEmergency(total_accel);
  }
}

void handleActiveAlert() {
  unsigned long currentMillis = millis();
  
  // Auto-stop if no one presses reset after duration
  if (currentMillis - alertStartTime >= ALARM_DURATION) {
    stopAlert("Alert Time-out");
    return;
  }

  // Flashing effect for Alert
  if (currentMillis - lastBeepTime >= BEEP_INTERVAL) {
    lastBeepTime = currentMillis;
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState);
    digitalWrite(LED_PIN, buzzerState);
    
    lcd.setCursor(0,0);
    if(buzzerState) {
      lcd.print("!! EMERGENCY !! "); 
    } else {
      lcd.print("   HELP NEEDED  ");
    }
  }
}

void stopAlert(String message) {
  isAlertActive = false;
  digitalWrite(BUZZER_PIN, LOW); 
  digitalWrite(LED_PIN, LOW);
  displayMedicalInfo(); 
  Serial.print(F("ALERT STOPPED: "));
  Serial.println(message);
}

void triggerEmergency(float force) {
  lcd.clear();
  if (force > 0) {
    lcd.print("IMPACT DETECTED!");
  } else {
    lcd.print("PANIC BUTTON!");
  }
  
  Serial.println(F("\n--- SOS ALERT SENT ---"));
  
  if (gps.location.isValid()) {
    Serial.print(F("LAT: ")); Serial.println(gps.location.lat(), 6);
    Serial.print(F("LNG: ")); Serial.println(gps.location.lng(), 6);
    Serial.print(F("Map: https://www.google.com/maps?q="));
    Serial.print(gps.location.lat(), 6);
    Serial.print(",");
    Serial.println(gps.location.lng(), 6);
  } else {
    Serial.println(F("Location: GPS Signal Weak..."));
  }
}

void logLocation() {
  if (gps.location.isValid() && !isAlertActive) {
    Serial.print(F("[LIVE TRACK] Lat: ")); Serial.print(gps.location.lat(), 6);
    Serial.print(F(" | Lng: ")); Serial.println(gps.location.lng(), 6);
  }
}
