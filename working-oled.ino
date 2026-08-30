#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>

// =========================================================================
//  USER CONFIGURATION
// =========================================================================
const char* WIFI_SSID = "internet";
const char* WIFI_PASS = "jakederg";

WiFiServer server(80);

// Hardware Pins & Calibration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SOIL_PIN 34    // ESP32 Analog Pin for Soil Moisture
#define TEMP_PIN 35    // ESP32 Analog Pin for 2-Pin Air Thermistor

const int DRY_VALUE = 3000;   // ESP32 ADC reads up to 4095
const int WET_VALUE = 1200;    

// Global Variables
float temperature = 0.0;       
int soilMoisturePercent = 0;
bool isUnplugged = false;

// Non-blocking Timers
unsigned long previousSensorMillis = 0;
const long sensorInterval = 2000; 
unsigned long previousFlashMillis = 0;
const long flashInterval = 300; 
bool showAlertState = true;

void setup() {
  Serial.begin(9600);
  
  // ESP32 Default I2C Pins (SDA = 21, SCL = 22)
  Wire.begin(21, 22);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;);
  }
  Wire.setClock(400000); 
  
  pinMode(SOIL_PIN, INPUT);
  pinMode(TEMP_PIN, INPUT);
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("Connecting Wi-Fi...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    retries++;
  }

  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. NON-BLOCKING SENSOR READINGS
  if (currentMillis - previousSensorMillis >= sensorInterval) {
    previousSensorMillis = currentMillis;

    // Read 2-Pin Thermistor (Convert analog 0-4095 to an estimated Celsius value)
    int rawTemp = analogRead(TEMP_PIN);
    // Basic rough mapping for a standard 10k thermistor voltage divider:
    temperature = map(rawTemp, 4000, 500, 0, 50); 

    // Read Soil Moisture
    int rawSoil = analogRead(SOIL_PIN);
    isUnplugged = (rawSoil >= 4050); // ESP32 max pull-up value

    if (!isUnplugged) {
      soilMoisturePercent = map(rawSoil, DRY_VALUE, WET_VALUE, 0, 100);
      soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
    }
  }

  if (currentMillis - previousFlashMillis >= flashInterval) {
    previousFlashMillis = currentMillis;
    showAlertState = !showAlertState; 
  }

  // 2. WEB SERVER HANDLING
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String requestUrl = "";
    boolean currentLineIsBlank = true;

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        if (requestUrl.length() < 30 && c != '\r' && c != '\n') {
          requestUrl += c;
        }

        if (c == '\n' && currentLineIsBlank) {
          break;
        }
        
        if (c == '\n') {
          currentLineIsBlank = true;
          currentLine = "";
        } else if (c != '\r') {
          currentLine += c;
          currentLineIsBlank = false;
        }
      }
    }

    // JSON API ENDPOINT
    if (requestUrl.indexOf("GET /data") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();

      client.print("{\"t\":");
      client.print(temperature, 1);
      client.print(",\"m\":");
      client.print(soilMoisturePercent);
      client.print(",\"u\":");
      client.print(isUnplugged ? 1 : 0);
      client.println("}");
    } 
    // MAIN DASHBOARD HTML PAGE
    else {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();

      client.println("<!DOCTYPE html><html><head>");
      client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
      client.println("<style>");
      client.println("body { font-family: Arial, sans-serif; background: #121212; color: #fff; text-align: center; padding: 20px; }");
      client.println(".card { background: #1e1e1e; padding: 20px; border-radius: 12px; margin: 15px auto; max-width: 300px; }");
      client.println(".val { font-size: 2.5em; font-weight: bold; color: #4caf50; }");
      client.println(".alert { color: #f44336; }");
      client.println(".st { font-weight: bold; margin-top: 10px; }");
      client.println("</style></head><body>");

      client.println("<h2>ESP32 Plant Dashboard</h2>");
      
      client.println("<div class='card'><h3>Soil Moisture</h3>");
      client.println("<div id='mVal' class='val'>--%</div>");
      client.println("<div id='stText' class='st' style='color:#ffa500;'>Connecting...</div></div>");

      client.println("<div class='card'><h3>Air Temperature</h3>");
      client.println("<div id='tVal' class='val' style='color:#2196f3;'>-- &deg;C</div></div>");

      client.println("<script>");
      client.println("function fetchData() {");
      client.println("  fetch('/data?t=' + Date.now())");
      client.println("    .then(res => res.json())");
      client.println("    .then(data => {");
      client.println("      document.getElementById('tVal').innerHTML = data.t.toFixed(1) + ' &deg;C';");
      client.println("      let m = document.getElementById('mVal');");
      client.println("      let s = document.getElementById('stText');");
      client.println("      if(data.u === 1) {");
      client.println("        m.innerText = 'DISCONN'; m.className = 'val alert';");
      client.println("        s.innerText = 'Sensor Unplugged'; s.style.color = '#f44336';");
      client.println("      } else if(data.m <= 20) {");
      client.println("        m.innerText = data.m + '%'; m.className = 'val alert';");
      client.println("        s.innerText = 'WATER THE PLANT!'; s.style.color = '#f44336';");
      client.println("      } else {");
      client.println("        m.innerText = data.m + '%'; m.className = 'val';");
      client.println("        s.innerText = 'Status: Healthy'; s.style.color = '#4caf50';");
      client.println("      }");
      client.println("    }).catch(e => {");
      client.println("      document.getElementById('stText').innerText = 'Reconnecting...';");
      client.println("    });");
      client.println("}");
      client.println("fetchData();");
      client.println("setInterval(fetchData, 2000);"); 
      client.println("</script>");

      client.println("</body></html>");
    }

    delay(1);
    client.stop();
  }

  // 3. PHYSICAL OLED SCREEN RENDER
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(15, 0);
  display.print("ESP32 PLANT MONITOR");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.print("Air Temp: ");
  display.print(temperature, 1);
  display.print(" C");

  if (!isUnplugged && soilMoisturePercent <= 20) {
    if (showAlertState) {
      display.fillRect(0, 32, 128, 32, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(18, 44);
      display.print("WATER THE PLANT!");
      display.setTextColor(SSD1306_WHITE);
    }
  } 
  else {
    display.setCursor(0, 34);
    display.print("Soil Moist: ");
    if (isUnplugged) {
      display.setTextSize(1);
      display.setCursor(68, 34);
      display.print("DISCONN");
    } else {
      display.setTextSize(2); 
      display.setCursor(68, 30);
      display.print(soilMoisturePercent);
      display.setTextSize(1);
      display.print("%");
    }

    display.drawRect(0, 52, 128, 10, SSD1306_WHITE);
    if (!isUnplugged) {
      int barWidth = map(soilMoisturePercent, 0, 100, 0, 124);
      display.fillRect(2, 54, barWidth, 6, SSD1306_WHITE);
    }
  }

  display.display();
}