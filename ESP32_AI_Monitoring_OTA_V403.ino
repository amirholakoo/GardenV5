#include <esp_task_wdt.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Update.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32


// Replace with your network credentials
const char* host = "esp32";
const char* ssid = "XXX";
const char* password = "XXX";
int moist_count = 0;
int dry_count = 0;
int germination_count = 0;
float wet_soil_ratio = 0;
float germination_avg = 0;
uint16_t light_level = 0;
float temperature = 0;
float humidity = 0;
float pressure = 0;

WebServer server(80);

// Relay connected to GPIO pin
const int relayPin4 = 33;
const int relayPin2 = 26;
const int relayPin0 = 27;
//const int relayPins[] = {11, 10, 9, 13};

const int builtInLedPin = 5;

// Create BME280 instance
Adafruit_BME280 bme;

// Create BH1750 instance
BH1750 lightMeter;

// Create OLED instance
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//////////////////////////////////////////////
//////////////////////////////////////////////
/*
 * Login page
 */

const char* loginIndex =
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>ESP32 Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
             "<td>Username:</td>"
             "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='admin' && form.pwd.value=='admin')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";

/*
 * Server Index Page
 */

const char* serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Update'>"
    "</form>"
 "<div id='prg'>progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')"
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";
 
//////////////////////////////////////////////
//////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  //WatchDOG statrs HERE:
  esp_task_wdt_init(1800, true); // 4 hours * 60 minutes/hour * 60 seconds/minute = 14400 seconds
  esp_task_wdt_add(NULL);
  Serial.println("WatchDog...30MIN");
  
  delay(5000);
  //pinMode(builtInLedPin, OUTPUT);
  //digitalWrite(builtInLedPin, LOW);
  /*/ Set up the relay pins
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], HIGH);  // Initially turn off the pumps
  }
  */
  pinMode(relayPin4, OUTPUT);
  digitalWrite(relayPin4, HIGH);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin2, HIGH);
  pinMode(relayPin0, OUTPUT);
  digitalWrite(relayPin0, HIGH);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to Wi-Fi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  
  // Initialize BME280 sensor
  if (!bme.begin(0x77)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  // Initialize BH1750 sensor
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println("BH1750 Advanced begin");

  // Initialize OLED
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Initializing...");
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,16);
  display.println(WiFi.localIP());
  display.display();
  delay(5000);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("WatchDog...30MIN");
  display.display();
  delay(5000);
  

  // Set up server routes
  server.on("/capture", handleCapture);
  server.on("/control_pump", handleControlPump);
  server.on("/sensor_data", handleSensorData);
  server.on("/update_data", HTTP_POST, handleUpdateData);

  
  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) { //http://esp32.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
  /*return index page which is stored in serverIndex */
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", loginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  
  // Start the server
  server.begin();
  Serial.println("Servers Are Running:");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println("Servers Are Running:");
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,16);
  display.println(WiFi.localIP());
  display.println("/capture");
  display.println("/control_pump");
  display.println("/sensor_data");
  display.println("/update_data");
  display.display();
  delay(5000);

}


//////////////////////////////////////////////////////////////////////////
void loop() {
  server.handleClient();
  delay(1);
  //Serial.println("Server handling Client");
  handleSensorData();
  //Serial.println("Server handling SensorData");
  delay(1 * 1000); // XX SECONDS 
  handleUpdateData();
  //Serial.println("Server handling UpdateData");
  delay(1 * 1000);
  handleControlPump();
  //Serial.println("Server handling ControlPump");
  delay(1 * 1000);
  //handleUpdateData();
  //delay(10 * 1000);


  
}


////////////////////////////////////////////////////////////////////
void handleCapture() {
  // Implement your image capture code here
  server.send(200, "text/plain", "Capture not implemented.");
}

void handleControlPump() {
  String state = server.arg("state");
  String pump = server.arg("pump");
  int pumpNum = pump.toInt();
  
  if (state == "on") {
    
    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("PUMP: ON");
    display.setCursor(0,16);
    display.println("Watering Started...");
    display.print("Light: ");
    display.println(light_level);
    display.print("Dry Pixels: ");
    display.println(dry_count);
    //display.println(pumpNum);
    display.display();
    delay(100);
    
    digitalWrite(relayPin4, LOW);
    delay(15000);
    digitalWrite(relayPin2, LOW);
    delay(15000);
    digitalWrite(relayPin0, LOW);
    delay(15000);
    
    Serial.println("PUMP ON");
    delay(100);
    
    //digitalWrite(builtInLedPin, LOW); // Turn on the LED (active low)
    //Serial.println("LED ON");
    //delay(100);

    server.send(200, "text/plain", "Pump" + pump + " turned on");
    
    
    
  } else if (state == "off") {
    digitalWrite(relayPin4, HIGH);
    delay(5000);
    digitalWrite(relayPin2, HIGH);
    delay(5000);
    digitalWrite(relayPin0, HIGH);
    delay(5000);
    //Serial.println("PUMP OFF");
    //delay(100);
    //digitalWrite(builtInLedPin, HIGH); 
    //Serial.println("LED OFF");
    //delay(100);
    
    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("PUMP: OFF");
    display.setCursor(0,16);
    display.println("Watering DONE!");
    display.println(pumpNum);
    display.display();
    delay(10000);
    
    server.send(200, "text/plain", "Pump" + pump + " turned off");

    esp_task_wdt_init(1, true); // 1 seconds
    esp_task_wdt_add(NULL);
    
  } else {
    server.send(400, "text/plain", "Invalid state");
    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Invalid state");
    display.display();
    delay(1000);
  }
}

void handleSensorData() {
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure() / 100.0F;
  light_level = lightMeter.readLightLevel();

  String json = "{\"temperature\": " + String(temperature) + ",";
  json += "\"humidity\": " + String(humidity) + ",";
  json += "\"pressure\": " + String(pressure) + ",";
  json += "\"light_level\": " + String(light_level) + "}";

  server.send(200, "application/json", json);

    // Update OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Germination %: ");
  display.println(germination_avg);
  display.print("Moist/Total %: ");
  display.println(wet_soil_ratio);
  display.setCursor(0,16);
  display.print("Temp    : ");
  display.print(temperature);
  display.println("   C");
  display.print("Humidity: ");
  display.print(humidity);
  display.println("   %");
  display.print("Pressure: ");
  display.print(pressure);
  display.println("  hPa");
  display.print("Light   : ");
  display.print(light_level);
  display.println("    lx");
  display.print("Moist    :");
  display.println(moist_count);
  display.print("Dry      :");
  display.println(dry_count);
  display.display();
}

float handleUpdateData() {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, server.arg("plain"));

  if (error) {
    server.send(400, "application/json", "{\"error\": \"Invalid JSON\"}");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Updating... ");
    display.display();
    
    // Store the received values in variables
    moist_count = doc["moist_count"];
    dry_count = doc["dry_count"];
    germination_count = doc["germination_count"];
    wet_soil_ratio = doc["wet_soil_ratio"];
    germination_avg = doc["germination_avg"];

    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println("Germination Ratio: ");
    display.println(germination_avg);
    display.setCursor(0,16);
    display.print("Moist: ");
    display.println(moist_count);
    display.print("Dry  : ");
    display.println(dry_count);
    display.print("Ratio: % ");
    display.println(wet_soil_ratio);
    display.println("Germination Pixels: ");
    display.println(germination_count);
    display.display();
    server.send(200, "application/json", "{\"status\": \"ok\"}");

  }

  return moist_count;
}
