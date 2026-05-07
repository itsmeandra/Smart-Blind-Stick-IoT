#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <TinyGPSPlus.h>

//────────── WIFI SETTINGS ──────────
const char* ssid = "WIFI_NAME"; 
const char* password = "WIFI_PASSWORD";

//────────── TELEGRAM SETTINGS ──────────
#define BOT_TOKEN "TELEGRAM_BOT_TOKEN"
#define CHAT_ID "TELEGRAM_CHAT_ID"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

//────────── PIN CONFIGURATION ──────────
const int trigPin = 5;
const int echoPin = 18;
const int vibrationPin = 19;
const int buzzerPin = 21;

// GPS RX/TX Pins (Using ESP32 Hardware Serial 2)
#define RXD2 16 
#define TXD2 17

TinyGPSPlus gps;

//────────── TIME VARIABLES (NON-BLOCKING) ──────────
// Telegram Update Interval (Set to 10 minutes)
unsigned long previousTelegramMillis = 0;
const unsigned long telegramInterval = 10 * 60 * 1000UL; 

// Buzzer Tempo Variables
unsigned long previousBuzzerMillis = 0;
int buzzerStatus = LOW;
int buzzerInterval = 0; // 0 = Off

void setup() {
  Serial.begin(115200); // Debugging Serial Monitor
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); // GPS Communication

  // Pin Initialization
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(vibrationPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  // Start WiFi connection in the background
  WiFi.begin(ssid, password);
  
  // Required for Telegram on ESP32
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); 
  
  Serial.println("Smart Blind Stick System Active!");
}

void loop() {
  // 1. READ GPS DATA CONTINUOUSLY
  // Captures satellite signals whenever available
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  // 2. ULTRASONIC DISTANCE DETECTION
  long distance = readDistance();

  // 3. RUN ALERT LOGIC (PRIMARY SAFETY)
  handleAlerts(distance);

  // 4. RUN TELEGRAM LOGIC (SEND UPDATES PERIODICALLY)
  sendTelegramUpdate();
}

//────────── SUPPORTING FUNCTIONS ──────────

// Function to read HC-SR04 sensor with a timeout to prevent hanging
long readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // 30,000µs timeout (~5 meters) ensures the system doesn't stall if no reflection is found
  long duration = pulseIn(echoPin, HIGH, 30000); 
  if (duration == 0) return 999; // If no reflection, assume path is clear
  
  return duration * 0.034 / 2; // Convert to Centimeters
}

// Function to handle vibration and buzzer tempo based on distance
void handleAlerts(long distance) {
  unsigned long currentMillis = millis(); 

  if (distance > 80) {
    // ---- SAFE CONDITION ----
    analogWrite(vibrationPin, 0); // Stop vibration
    buzzerInterval = 0;           // Stop buzzer
  } 
  else if (distance <= 80 && distance > 50) {
    // ---- EARLY WARNING ----
    analogWrite(vibrationPin, 100); // Low vibration (PWM: 0-255)
    buzzerInterval = map(jarak, 51, 80, 50, 600);           // Slow tempo (every 500ms)
  } 
  else if (distance <= 50) {
    // ---- DANGER CONDITION ----
    analogWrite(vibrationPin, 255); // Max vibration
    buzzerInterval = -1;            // Continuous beep
  }

  // Non-blocking buzzer logic
  if (buzzerInterval > 0) {
    if (currentMillis - previousBuzzerMillis >= buzzerInterval) {
      previousBuzzerMillis = currentMillis;
      buzzerStatus = !buzzerStatus; // Toggle buzzer state
      digitalWrite(buzzerPin, buzzerStatus);
    }
  } else if (buzzerInterval == -1) {
    digitalWrite(buzzerPin, HIGH); // Continuous sound
  } else {
    digitalWrite(buzzerPin, LOW);  // Turn off buzzer
  }
}

// Function to send GPS coordinates to Telegram every 15 minutes
void sendTelegramUpdate() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousTelegramMillis >= telegramInterval) {
    previousTelegramMillis = currentMillis;

    if (WiFi.status() == WL_CONNECTED) {
      // Ensure GPS has a valid satellite fix
      if (gps.location.isValid()) {
        String message = "📍 Location Update (10-Min Interval):\n";
        message += "The Smart Stick user is currently at:\n";
        message += "https://maps.google.com/?q=";
        message += String(gps.location.lat(), 6) + ",";
        message += String(gps.location.lng(), 6);

        bot.sendMessage(CHAT_ID, message, "");
        Serial.println("Telegram update sent successfully.");
      } else {
        Serial.println("Telegram failed: GPS signal not valid yet (Cold Start).");
      }
    } else {
      Serial.println("Telegram failed: WiFi not connected.");
    }
  }
}