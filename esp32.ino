#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#define OLED_LARGURA 128
#define OLED_ALTURA 64
#define OLED_RESET 4
#define SERVO1_PIN 18

// WiFi Configuration
const char* ap_ssid = "MedicineDispenser";
const char* ap_password = "12345678";

// Hardware components
Adafruit_SSD1306 display(OLED_LARGURA, OLED_ALTURA, &Wire, OLED_RESET);
WiFiUDP udp;
NTPClient ntp(udp, "pool.ntp.org", 0, 60000);
Servo servo1;
WebServer server(80);
Preferences preferences;

// Dispensing schedule structure
struct DispenseTime {
  int hour;
  int minute;
  int pills;
  bool active;
};

DispenseTime schedule[10]; // Maximum 10 dispensing times per day
int scheduleCount = 0;
bool lastDispenseCheck[10] = {false};

String currentTime;
String displayMessage1 = "";
String displayMessage2 = "";
unsigned long messageDisplayTime = 0;
bool showingMessage = false;
bool isConfigured = false;
int timezone_offset = 0;
bool wifiConnected = false;

void setup() {
  Serial.begin(115200);
  
  // Initialize preferences for persistent storage
  preferences.begin("med_dispenser", false);
  loadConfiguration();
  
  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erro ao inicializar o display OLED");
    while (true);
  }
  
  display.clearDisplay();
  Serial.println("Display iniciado com sucesso.");
  
  // Initialize servo
  servo1.attach(SERVO1_PIN);
  servo1.write(0); // Closed position
  
  // Setup WiFi
  setupWiFi();
  
  // Setup web server
  setupWebServer();
  
  // Start NTP client
  ntp.begin();
  
  Serial.println("Medicine Dispenser initialized");
  showTemporaryMessage("Sistema", "Iniciado!");
}

void loop() {
  server.handleClient();
  
  // Update time if WiFi connected
  if (wifiConnected) {
    ntp.update();
  }
  
  currentTime = getCurrentTimeString();
  
  // Check for dispensing times
  if (isConfigured && wifiConnected) {
    checkDispenseSchedule();
  }
  
  // Update display continuously
  updateDisplay();
  
  delay(100);
}

void setupWiFi() {
  showTemporaryMessage("WiFi", "Conectando...");
  
  // Try to connect to saved WiFi first
  String saved_ssid = preferences.getString("wifi_ssid", "");
  String saved_password = preferences.getString("wifi_password", "");
  
  if (saved_ssid.length() > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(saved_ssid.c_str(), saved_password.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      ntp.setTimeOffset(timezone_offset * 3600);
      ntp.forceUpdate();
      Serial.println("\nWiFi connected!");
      showTemporaryMessage("WiFi", "Conectado!");
    }
  }
  
  // Start AP mode regardless
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("WiFi AP started");
  Serial.println("SSID: " + String(ap_ssid));
  Serial.println("AP IP: " + WiFi.softAPIP().toString());
  
  if (!wifiConnected) {
    showTemporaryMessage("Modo AP", "Ativo");
  }
}

void setupWebServer() {
  // Enable CORS for all routes
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      sendCORSHeaders();
      server.send(200);
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
  
  // Get device information
  server.on("/info", HTTP_GET, handleDeviceInfo);
  
  // Configure WiFi connection
  server.on("/configure_wifi", HTTP_POST, handleWiFiConfig);
  
  // Configure medication schedule
  server.on("/configure_schedule", HTTP_POST, handleScheduleConfig);
  
  // Manual pill dispensing
  server.on("/dispense", HTTP_POST, handleManualDispense);
  
  // Get current schedule
  server.on("/schedule", HTTP_GET, handleGetSchedule);
  
  server.begin();
  Serial.println("Web server started on port 80");
}

void handleDeviceInfo() {
  sendCORSHeaders();
  
  DynamicJsonDocument doc(1024);
  doc["device_name"] = "Medicine Dispenser";
  doc["version"] = "1.0";
  doc["configured"] = isConfigured;
  doc["current_time"] = currentTime;
  doc["schedule_count"] = scheduleCount;
  doc["wifi_connected"] = wifiConnected;
  doc["ap_ip"] = WiFi.softAPIP().toString();
  
  if (wifiConnected) {
    doc["wifi_ip"] = WiFi.localIP().toString();
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleWiFiConfig() {
  sendCORSHeaders();
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data received\"}");
    return;
  }
  
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, server.arg("plain"));
  
  String ssid = doc["ssid"];
  String password = doc["password"];
  timezone_offset = doc["timezone_offset"];
  
  showTemporaryMessage("Conectando", ssid);
  
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    ntp.setTimeOffset(timezone_offset * 3600);
    ntp.forceUpdate();
    
    // Save WiFi credentials
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_password", password);
    preferences.putInt("timezone", timezone_offset);
    
    showTemporaryMessage("WiFi OK!", WiFi.localIP().toString());
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"WiFi connected\"}");
  } else {
    showTemporaryMessage("WiFi Erro", "Verificar dados");
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Failed to connect to WiFi\"}");
  }
}

void handleScheduleConfig() {
  sendCORSHeaders();
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data received\"}");
    return;
  }
  
  DynamicJsonDocument doc(2048);
  deserializeJson(doc, server.arg("plain"));
  
  scheduleCount = 0;
  JsonArray times = doc["schedule"];
  
  for (JsonObject time : times) {
    if (scheduleCount < 10) {
      schedule[scheduleCount].hour = time["hour"];
      schedule[scheduleCount].minute = time["minute"];
      schedule[scheduleCount].pills = time["pills"];
      schedule[scheduleCount].active = true;
      scheduleCount++;
    }
  }
  
  saveConfiguration();
  isConfigured = true;
  
  // Reset daily tracking
  for (int i = 0; i < 10; i++) {
    lastDispenseCheck[i] = false;
  }
  
  showTemporaryMessage("Horarios", "Configurados!");
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Schedule configured\"}");
}

void handleManualDispense() {
  sendCORSHeaders();
  
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No data received\"}");
    return;
  }
  
  DynamicJsonDocument doc(256);
  deserializeJson(doc, server.arg("plain"));
  
  int pills = doc["pills"];
  dispensePills(pills);
  
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Pills dispensed\"}");
}

void handleGetSchedule() {
  sendCORSHeaders();
  
  DynamicJsonDocument doc(2048);
  JsonArray scheduleArray = doc.createNestedArray("schedule");
  
  for (int i = 0; i < scheduleCount; i++) {
    JsonObject timeObj = scheduleArray.createNestedObject();
    timeObj["hour"] = schedule[i].hour;
    timeObj["minute"] = schedule[i].minute;
    timeObj["pills"] = schedule[i].pills;
    timeObj["active"] = schedule[i].active;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void checkDispenseSchedule() {
  int currentHour = ntp.getHours();
  int currentMinute = ntp.getMinutes();
  
  for (int i = 0; i < scheduleCount; i++) {
    if (schedule[i].active && 
        schedule[i].hour == currentHour && 
        schedule[i].minute == currentMinute &&
        !lastDispenseCheck[i]) {
      
      showTemporaryMessage("Dispensando", formatTime(currentHour, currentMinute));
      dispensePills(schedule[i].pills);
      lastDispenseCheck[i] = true;
      
      Serial.println("Automatic dispense at " + formatTime(currentHour, currentMinute));
    }
  }
  
  // Reset daily tracking at midnight
  if (currentHour == 0 && currentMinute == 0) {
    for (int i = 0; i < 10; i++) {
      lastDispenseCheck[i] = false;
    }
  }
}

void dispensePills(int pillCount) {
  Serial.println("Dispensing " + String(pillCount) + " pills");
  
  for (int i = 0; i < pillCount; i++) {
    showTemporaryMessage("Comprimido", String(i + 1) + "/" + String(pillCount));
    
    // Open compartment
    servo1.write(90);
    delay(1000);
    
    // Close compartment
    servo1.write(0);
    delay(2000);
  }
  
  showTemporaryMessage("Concluido!", String(pillCount) + " comprimidos");
  delay(3000);
}

String getCurrentTimeString() {
  if (wifiConnected) {
    return ntp.getFormattedTime();
  } else {
    return "--:--:--";
  }
}

String formatTime(int hour, int minute) {
  return String(hour < 10 ? "0" : "") + String(hour) + ":" + 
         String(minute < 10 ? "0" : "") + String(minute);
}

// Main display function - always shows time prominently
void updateDisplay() {
  display.clearDisplay();
  
  // Check if we should show a temporary message
  if (showingMessage && (millis() - messageDisplayTime < 3000)) {
    // Show temporary message with time at bottom
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 8);
    display.println(displayMessage1);
    display.setCursor(0, 24);
    display.println(displayMessage2);
    
    // Always show current time at bottom in smaller font
    display.setCursor(0, 48);
    display.setTextSize(1);
    display.println("Hora: " + currentTime);
  } else {
    // Normal display mode - time is prominent
    showingMessage = false;
    
    // Large time display
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(10, 8);
    display.println(currentTime);
    
    // Status information below
    display.setTextSize(1);
    display.setCursor(0, 32);
    
    if (wifiConnected) {
      display.println("WiFi: Conectado");
    } else {
      display.println("WiFi: Modo AP");
    }
    
    display.setCursor(0, 42);
    if (isConfigured && scheduleCount > 0) {
      display.println("Horarios: " + String(scheduleCount));
      
      // Show next scheduled time
      display.setCursor(0, 52);
      String nextTime = getNextScheduledTime();
      if (nextTime.length() > 0) {
        display.println("Proximo: " + nextTime);
      }
    } else {
      display.println("Nao configurado");
    }
  }
  
  display.display();
}

// Function to show temporary messages while keeping time visible
void showTemporaryMessage(String line1, String line2) {
  displayMessage1 = line1;
  displayMessage2 = line2;
  messageDisplayTime = millis();
  showingMessage = true;
}

// Get the next scheduled time for display
String getNextScheduledTime() {
  if (scheduleCount == 0 || !wifiConnected) return "";
  
  int currentHour = ntp.getHours();
  int currentMinute = ntp.getMinutes();
  int currentTotalMinutes = currentHour * 60 + currentMinute;
  
  int nextTotalMinutes = 9999; // Large number
  
  for (int i = 0; i < scheduleCount; i++) {
    if (schedule[i].active) {
      int scheduleTotalMinutes = schedule[i].hour * 60 + schedule[i].minute;
      
      // If this time is later today
      if (scheduleTotalMinutes > currentTotalMinutes) {
        if (scheduleTotalMinutes < nextTotalMinutes) {
          nextTotalMinutes = scheduleTotalMinutes;
        }
      }
    }
  }
  
  // If no time found for today, get first time tomorrow
  if (nextTotalMinutes == 9999) {
    for (int i = 0; i < scheduleCount; i++) {
      if (schedule[i].active) {
        int scheduleTotalMinutes = schedule[i].hour * 60 + schedule[i].minute;
        if (scheduleTotalMinutes < nextTotalMinutes) {
          nextTotalMinutes = scheduleTotalMinutes;
        }
      }
    }
  }
  
  if (nextTotalMinutes == 9999) return "";
  
  int nextHour = nextTotalMinutes / 60;
  int nextMinute = nextTotalMinutes % 60;
  
  return formatTime(nextHour, nextMinute);
}

void sendCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void saveConfiguration() {
  preferences.putInt("schedule_count", scheduleCount);
  preferences.putBool("configured", isConfigured);
  
  for (int i = 0; i < scheduleCount; i++) {
    String key = "sched_" + String(i);
    String value = String(schedule[i].hour) + "," + 
                   String(schedule[i].minute) + "," + 
                   String(schedule[i].pills);
    preferences.putString(key.c_str(), value);
  }
}

void loadConfiguration() {
  isConfigured = preferences.getBool("configured", false);
  scheduleCount = preferences.getInt("schedule_count", 0);
  timezone_offset = preferences.getInt("timezone", 0);
  
  for (int i = 0; i < scheduleCount; i++) {
    String key = "sched_" + String(i);
    String value = preferences.getString(key.c_str(), "");
    
    if (value.length() > 0) {
      int firstComma = value.indexOf(',');
      int secondComma = value.indexOf(',', firstComma + 1);
      
      schedule[i].hour = value.substring(0, firstComma).toInt();
      schedule[i].minute = value.substring(firstComma + 1, secondComma).toInt();
      schedule[i].pills = value.substring(secondComma + 1).toInt();
      schedule[i].active = true;
    }
  }
}