#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 2
#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_PIN A0 

// Soil Sensor Calibration Values
const int DRY_VALUE = 850;   // Calibrated typical dry value under pullup conditions
const int WET_VALUE = 200;   

unsigned long previousDHTMillis = 0;
const long dhtInterval = 2000; 
float temperature = 0.0;       

void setup() {
  Serial.begin(9600);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;);
  }
  Wire.setClock(400000); 
  
  dht.begin();
  
  // FIX: Enable the internal pull-up resistor on the analog pin.
  // This stops the pin from floating in mid-air when unplugged!
  pinMode(SOIL_PIN, INPUT_PULLUP);
  
  temperature = dht.readTemperature();
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousDHTMillis >= dhtInterval) {
    previousDHTMillis = currentMillis;
    float newTemp = dht.readTemperature();
    if (!isnan(newTemp)) {
      temperature = newTemp; 
    }
  }

  // Read raw value
  int rawSoil = analogRead(SOIL_PIN);
  
  bool isUnplugged = false;
  int soilMoisturePercent = 0;

  // With INPUT_PULLUP active, an unplugged pin will pin itself hard to 1023.
  // Most resistive sensors sit around 1010-1015 even in completely bone-dry air.
  if (rawSoil >= 1020) {
    isUnplugged = true;
  } else {
    // Process real values if plugged in
    soilMoisturePercent = map(rawSoil, DRY_VALUE, WET_VALUE, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
  }

  display.clearDisplay();

  // Header
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.print("LIVE PLANT DATA");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // Air Temperature
  display.setCursor(0, 18);
  display.print("Air Temp: ");
  if (isnan(temperature) || temperature == 0.0) {
    display.print("--- C");
  } else {
    display.print(temperature, 1);
    display.print(" C");
  }

  // Soil Moisture Display Logic
  display.setCursor(0, 34);
  display.print("Soil Moist: ");
  
  if (isUnplugged) {
    display.setTextSize(1);
    display.setCursor(68, 34);
    display.print("DISCONN"); // Explicit warning when unplugged!
  } else {
    display.setTextSize(2); 
    display.setCursor(68, 30);
    display.print(soilMoisturePercent);
    display.setTextSize(1);
    display.print("%");
  }

  // Progress Bar Layout
  display.drawRect(0, 52, 128, 10, SSD1306_WHITE);
  if (!isUnplugged) {
    int barWidth = map(soilMoisturePercent, 0, 100, 0, 124);
    display.fillRect(2, 54, barWidth, 6, SSD1306_WHITE);
  }

  display.display(); 
}
