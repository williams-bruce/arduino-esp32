#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <ESP32Servo.h>

#define OLED_LARGURA 128
#define OLED_ALTURA 64
#define OLED_RESET 4

#define SERVO1_PIN 18
#define SERVO2_PIN 19

const char* ssid = "Williams";
const char* password = "oi bruce :)";


Adafruit_SSD1306 display(OLED_LARGURA, OLED_ALTURA, &Wire, OLED_RESET);
WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000);

Servo servo1;
Servo servo2;

String hora;

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  delay(2000);
  ntp.begin();
  ntp.forceUpdate();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erro ao inicializar o display OLED");
    while (true);
  }

  display.clearDisplay();
  Serial.println("Display iniciado com sucesso.");

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  servo1.write(0);
  servo2.write(0);
}

void loop() {
  hora = ntp.getFormattedTime();
  
  // Atualiza display
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 8);
  display.println(hora);
  display.display();

  // Movimento de teste simultâneo dos dois servos
  for (int pos = 0; pos <= 30; pos += 2) {
    servo1.write(pos);
    servo2.write(180 - pos);
    delay(148);
  }

  delay(1000);
}
