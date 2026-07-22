// ==========================================
// 1. LIBRARIES & INCLUDES
// ==========================================
#include <WiFi.h>           // Core ESP32 library for WiFi connectivity
#include <time.h>           // Standard C time library for NTP time synchronization
#include <HTTPClient.h>     // Library to make HTTP GET requests to external APIs
#include <ArduinoJson.h>    // Library to parse JSON responses from external APIs
#include "DHT.h"            // Library for DHT11 temperature and humidity sensor
#include <FastLED.h>        // Library to control WS2812B addressable RGB LEDs
#include <Preferences.h>    // ESP32 library to save variables to Non-Volatile Storage (NVS)

// ==========================================
// 2. NETWORK & API CONFIGURATION (Hard-coding)
// ==========================================
const char* ssid = "YOUR_WIFI_SSID";              // WiFi network SSID
const char* password = "YOUR_WIFI_PASSWORD";                      // WiFi network password
const char* ntpServer = "pool.ntp.org";                   // Global NTP server for time syncing
const long  gmtOffset_sec = 10800;                        // GMT offset in seconds (10800s = GMT+3)
const int   daylightOffset_sec = 0;                       // Daylight saving time offset in seconds

String openWeatherApiKey = "YOUR_OPENWEATHER_API_KEY"; // OpenWeatherMap API key
float currentLat = 0.0;                                   // Stores dynamically fetched Latitude
float currentLon = 0.0;                                   // Stores dynamically fetched Longitude

// ==========================================
// 3. HARDWARE PIN DEFINITIONS
// ==========================================
// Nextion Display Serial Pins (Hardware Serial 2)
#define RXD2 16  // RX pin for Nextion communication
#define TXD2 17  // TX pin for Nextion communication

// DHT11 Sensor
#define DHTPIN 4       // Data pin connected to DHT11
#define DHTTYPE DHT11  // Explicitly defining the sensor type as DHT11
DHT dht(DHTPIN, DHTTYPE); // Initializing the DHT object

// Analog & Digital Sensors
#define MQ135_PIN 35   // Analog input pin for MQ135 Air Quality sensor
#define LDR_PIN 32     // Digital/Analog pin for Light Dependent Resistor
#define PIR_PIN 27     // Digital input pin for PIR motion sensor

// Addressable LED Strip
#define LED_PIN     13       // Data pin for WS2812B LED strip
#define NUM_LEDS    32       // Total number of LEDs on the strip
#define LED_TYPE    WS2812B  // LED chipset type
#define COLOR_ORDER GRB      // Color channel order (Green, Red, Blue)
CRGB leds[NUM_LEDS];         // Array holding the color data for each LED

// Gesture, Audio, & Analog Control
#define TRIG_PIN    5   // Output pin for Ultrasonic sensor trigger
#define ECHO_PIN    18  // Input pin for Ultrasonic sensor echo
#define AUDIO_PIN   25  // PWM output pin for a buzzer/speaker
#define POT_PIN     34  // Analog input pin for potentiometer (volume control)

// ==========================================
// 4. GLOBAL VARIABLES & STATE TRACKING
// ==========================================
Preferences preferences; // Object to handle saving/loading data to flash memory

// Timing & Timeout Variables
unsigned long lastDisplayUpdate = 0;    // Tracks last time the Nextion UI was updated
unsigned long lastWeatherUpdate = 0;    // Tracks last time weather API was called
unsigned long lastMotionTime = 0;       // Tracks last time PIR detected motion
const unsigned long screenTimeout = 30000; // Time in ms before screen goes to sleep (30s)
unsigned long lastPingTime = 0;         // Rate limiter for ultrasonic sensor
unsigned long lastWifiRetryTime = 0;    // Rate limiter for WiFi reconnection attempts

// Device State Variables
int currentBrightness = 80;  // Screen brightness level (0-100)
int currentVolume = 60;      // System audio volume level (0-100)
int ledBrightness = 100;     // FastLED brightness level (0-255)
int ledSpeed = 50;           // Animation speed for dynamic LED modes
bool isAutoBright = false;   // Flag for LDR-based auto brightness
bool isScreenAwake = true;   // Flag tracking if the Nextion screen is awake

// UI State Variables
int currentPage = 0;         // Currently active page on the Nextion display
int currentLang = 0;         // Active language: 0 = English, 1 = Turkish

// Weather String Buffers (Displayed on UI)
String outTempStr = "-- C";                 // Outdoor temperature string
String outCondStr = "--";                   // Main weather condition string
String outMinMaxStr = "Feels like --";      // "Feels like" temperature string
String outWindStr = "Wind: -- km/h";        // Wind speed string
String outRunStr = "Running: --";           // Custom metric determining if weather is good for running
String outDescStr = "--";                   // Detailed weather description
String outHumStr = "Humidity: --%";         // Outdoor humidity string

// Multi-language UI Dictionaries (Index 0 = EN, Index 1 = TR)
String apiLangCodes[] = {"en", "tr"}; 
String lblPageTitle_Dict[] = {"Settings", "Ayarlar"};
String lblLang_Dict[]      = {"Language", "Dil"};
String lblBright_Dict[]    = {"Brightness", "Parlaklik"};
String lblSound_Dict[]     = {"Sound", "Ses"};
String lblTemp_Dict[]      = {"Temperature", "Sicaklik"};
String lblHum_Dict[]       = {"Humidity", "Nem"};
String lblPPM_Dict[]       = {"Air Quality", "Hava Kalitesi"};
String lblFeels_Dict[]     = {"Feels Like", "Hissedilen"};
String lblLight_Dict[]     = {"Light", "Isik"};
String lblWeather_Dict[]   = {"Weather", "Hava"};
String lblLightTitle_Dict[]= {"Light Control", "Isik Kontrolu"};
String lblModeTitle_Dict[] = {"Mode", "Mod"};
String lblStatic_Dict[]    = {"Static", "Sabit"};
String lblBreath_Dict[]    = {"Breathing", "Nefes Alma"};
String lblSnake_Dict[]     = {"Snake", "Yilan"};
String lblSpeed_Dict[]     = {"Speed", "Hiz"};

// Stopwatch Variables
bool isStopwatchRunning = false;       // Flag tracking stopwatch status
unsigned long stopwatchElapsedSeconds = 0; // Total seconds elapsed on stopwatch
unsigned long lastStopwatchTick = 0;   // Timestamp of the last stopwatch second tick

// Alarm & Timer Variables
String alarms[5] = {"0000", "0000", "0000", "0000", "0000"}; // Array holding 5 alarm times in HHMM format
int currentAlarmSlot = 0;      // Currently selected alarm slot for editing
String lastTriggeredTime = ""; // Prevents the same alarm from triggering multiple times in the same minute

String inputAlarmBuffer = "0000"; // Temporary buffer for user keypad input (Alarm)
String inputTimerBuffer = "000000"; // Temporary buffer for user keypad input (Timer: HHMMSS)
bool isTimerRunning = false;        // Flag tracking timer status
long timerRemainingSeconds = 0;     // Total seconds remaining on the countdown timer
unsigned long lastTimerTick = 0;    // Timestamp of the last timer second tick

// Lighting Control Variables
bool isLightOn = false;             // Flag tracking if LED strip is powered on
bool wasLightOnBeforeSleep = false; // Remembers LED state before screen went to sleep
int currentLightMode = 0;           // 0:Static, 1:Breath, 2:Snake, 3+:Gradients
CRGB currentColor = CRGB::White;    // Current active solid color
int currentColorIndex = 0;          // Index for cycling through quickColors array
CRGB quickColors[] = {CRGB::White, CRGB::Red, CRGB::Blue, CRGB::Green, CRGB::Purple, CRGB::Orange}; // Predefined gesture colors

// Gesture Variables
unsigned long handPresentStartTime = 0; // Timestamp when a hand was first detected by ultrasonic
unsigned long lastValidHandTime = 0;    // Timestamp of the last valid ultrasonic reading
bool isHandPresent = false;             // Flag indicating a hand is currently hovering
bool holdActionTriggered = false;       // Flag ensuring "hold" gestures only trigger once

// NVS Volume Saving Protection (Prevents flash memory wear out)
bool pendingVolSave = false;         // Flag indicating volume changed and needs saving
unsigned long lastVolChangeTime = 0; // Timestamp of the last potentiometer change

// Function Prototype
void fetchExternalWeather(); // Declared early so it can be called before its definition

// ==========================================
// 5. NEXTION COMMUNICATION HELPERS
// ==========================================

// Base function to send raw string commands to Nextion
void sendToNextion(String command) {
  Serial2.print(command);          // Send the text command
  Serial2.write(0xff);             // Nextion command terminator byte 1
  Serial2.write(0xff);             // Nextion command terminator byte 2
  Serial2.write(0xff);             // Nextion command terminator byte 3
}

// Helper to update a text attribute of a Nextion component
void sendTextToNextion(String objName, String textValue) {
  // Formats as: object.txt="value"
  sendToNextion(objName + ".txt=\"" + textValue + "\"");
}

// Helper to update an integer value attribute of a Nextion component
void sendNumToNextion(String objName, int numValue) {
  // Formats as: object.val=123
  sendToNextion(objName + ".val=" + String(numValue));
}

// ==========================================
// 6. SETTINGS & NVS HELPERS
// ==========================================

// Saves current user preferences to ESP32 flash memory
void saveSettings() {
  preferences.putInt("lang", currentLang);       // Save language index
  preferences.putInt("bright", currentBrightness); // Save screen brightness
  preferences.putBool("lOn", isLightOn);         // Save LED power state
  preferences.putInt("lBrt", ledBrightness);     // Save LED brightness level
  preferences.putInt("lSpd", ledSpeed);          // Save LED animation speed
  preferences.putInt("lMode", currentLightMode); // Save current LED animation mode
  preferences.putInt("cIdx", currentColorIndex); // Save current quick color index
}

// ==========================================
// 7. UI UPDATE FUNCTIONS
// ==========================================

// Pushes all cached external weather data strings to the Nextion display
void pushWeatherUI() {
    sendTextToNextion("txtOutTemp", outTempStr);
    sendTextToNextion("txtOutCond", outCondStr);
    sendTextToNextion("txtOutMinMax", outMinMaxStr);
    sendTextToNextion("txtOutWind", outWindStr);
    sendTextToNextion("txtOutRun", outRunStr);
    sendTextToNextion("txtOutPollen", "Pollen: --"); // Placeholder for future API integration
    sendTextToNextion("txtOutAQI", "AQI: --");       // Placeholder for future API integration
    sendTextToNextion("txtOutDesc", outDescStr);
    sendTextToNextion("txtOutHum", outHumStr);
    sendTextToNextion("txtOutRain", "Chance: --%");  // Placeholder for future API integration
    sendTextToNextion("txtOutRainTime", "--:--");    // Placeholder for future API integration
    sendTextToNextion("txtOutUV", "UV index: --");   // Placeholder for future API integration
}

// Translates UI text elements based on currentLang and currentPage
void updateScreenLanguage() {
  if (currentPage == 0) { // Home Page
    sendTextToNextion("lblLight", lblLight_Dict[currentLang]);
    sendTextToNextion("lblWeather", lblWeather_Dict[currentLang]);
  }
  else if (currentPage == 1) { // Environment Page
    sendTextToNextion("lblOutTemp", lblTemp_Dict[currentLang]);
    sendTextToNextion("lblInTemp", lblTemp_Dict[currentLang]);
    sendTextToNextion("lblInHum", lblHum_Dict[currentLang]);
    sendTextToNextion("lblOutAQI", lblPPM_Dict[currentLang]);
    sendTextToNextion("lblInAQI", lblPPM_Dict[currentLang]);
    sendTextToNextion("lblOutWea", lblWeather_Dict[currentLang]);
  }
  else if (currentPage == 2) { // Settings Page
    sendTextToNextion("lblPageTitle", lblPageTitle_Dict[currentLang]);
    sendTextToNextion("lblLang", lblLang_Dict[currentLang]);
    sendTextToNextion("lblBright", lblBright_Dict[currentLang]);
    sendTextToNextion("lblSound", lblSound_Dict[currentLang]);
  }
  else if (currentPage == 4) { // Lighting Control Page
    sendTextToNextion("lblPageTitle", lblLightTitle_Dict[currentLang]);
    sendTextToNextion("lblModeTitle", lblModeTitle_Dict[currentLang]);
    sendTextToNextion("lblModeStatic", lblStatic_Dict[currentLang]);
    sendTextToNextion("lblModeBreath", lblBreath_Dict[currentLang]);
    sendTextToNextion("lblModeSnake", lblSnake_Dict[currentLang]);
    sendTextToNextion("lblBrightTitle", lblBright_Dict[currentLang]);
    sendTextToNextion("lblSpeedTitle", lblSpeed_Dict[currentLang]);
  }
  fetchExternalWeather(); // Refresh weather string language from API
}

// Updates UI elements related to alarms, timers, and stopwatches
void updateClockUI() {
  // Format alarm buffer (HHMM -> HH:MM)
  String aFormatted = inputAlarmBuffer.substring(0,2) + ":" + inputAlarmBuffer.substring(2,4);
  sendTextToNextion("page3.txtAlarmTime", aFormatted);
  
  // Format timer buffer (HHMMSS -> HH:MM:SS)
  String tFormatted = inputTimerBuffer.substring(0,2) + ":" + inputTimerBuffer.substring(2,4) + ":" + inputTimerBuffer.substring(4,6);
  sendTextToNextion("page3.txtTmrTime", tFormatted); 

  // Format stopwatch seconds into HH:MM:SS string
  int sh = stopwatchElapsedSeconds / 3600;
  int sm = (stopwatchElapsedSeconds % 3600) / 60;
  int ss = stopwatchElapsedSeconds % 60;
  char sbuf[9];
  sprintf(sbuf, "%02d:%02d:%02d", sh, sm, ss);
  sendTextToNextion("page3.txtSWTime", String(sbuf));
}

// Maps incoming serial commands from Nextion to hex colors or gradient modes
void handleColorCommand(String cmd) {
  cmd.toUpperCase();             // Ensure command is uppercase for matching
  bool isGradient = false;       // Track if the command requires a multi-color gradient
  
  // Static color mappings
  if (cmd == "COLOR_C1") currentColor = 0xFFFFFF;
  else if (cmd == "COLOR_C2") currentColor = 0xE0FFFF;
  else if (cmd == "COLOR_M1") currentColor = 0xFFF0F5;
  else if (cmd == "COLOR_M2") currentColor = 0xFFDAB9;
  else if (cmd == "COLOR_M3") currentColor = 0xFFFF00;
  else if (cmd == "COLOR_M4") currentColor = 0xFFD700;
  else if (cmd == "COLOR_M5") currentColor = 0xFFA500;
  else if (cmd == "COLOR_M6") currentColor = 0xFF4500;
  else if (cmd == "COLOR_M7") currentColor = 0xFF0000;
  else if (cmd == "COLOR_M8") currentColor = 0xDC143C;
  else if (cmd == "COLOR_M9") currentColor = 0xFF1493;
  else if (cmd == "COLOR_M10") currentColor = 0xFFC0CB;
  else if (cmd == "COLOR_M11") currentColor = 0x8A2BE2;
  else if (cmd == "COLOR_M12") currentColor = 0x800080;
  else if (cmd == "COLOR_M13") currentColor = 0xBA55D3;
  else if (cmd == "COLOR_M14") currentColor = 0xDDA0DD;
  else if (cmd == "COLOR_M15") currentColor = 0x00FFFF;
  else if (cmd == "COLOR_M16") currentColor = 0x40E0D0;
  else if (cmd == "COLOR_M17") currentColor = 0x87CEFA;
  else if (cmd == "COLOR_M18") currentColor = 0x0000FF;
  else if (cmd == "COLOR_M19") currentColor = 0xADFF2F;
  else if (cmd == "COLOR_M20") currentColor = 0x00FF00;
  else if (cmd == "COLOR_M21") currentColor = 0x32CD32;
  else if (cmd == "COLOR_M22") currentColor = 0x006400;
  // Gradient mode mappings
  else if (cmd == "COLOR_M23") { currentLightMode = 3; isGradient = true; } 
  else if (cmd == "COLOR_M24") { currentLightMode = 4; isGradient = true; } 
  else if (cmd == "COLOR_M25") { currentLightMode = 5; isGradient = true; } 
  else if (cmd == "COLOR_M26") { currentLightMode = 6; isGradient = true; } 
  
  if (!isGradient) {
    currentLightMode = 0; // Reset to static mode if a solid color was chosen
    if (isLightOn) {      // Apply color immediately if lights are on
      fill_solid(leds, NUM_LEDS, currentColor); 
      FastLED.show();
    }
  }
  saveSettings(); // Commit the new color to memory
}

// Reads serial commands sent from Nextion buttons and executes associated logic
void listenToNextion() {
  if (Serial2.available()) {
    String data = Serial2.readStringUntil('\n'); // Read incoming command
    data.trim();                                 // Remove whitespace/newlines
    if (data.length() == 0) return;              // Ignore empty data

    // --- Language & Weather Commands ---
    if (data == "LANG_EN") { currentLang = 0; updateScreenLanguage(); saveSettings(); }
    else if (data == "LANG_TR") { currentLang = 1; updateScreenLanguage(); saveSettings(); }
    else if (data == "WEATHER_FORCE_UPDATE") { fetchExternalWeather(); }

    // --- Page Navigation Tracking ---
    else if (data == "PAGE_0") { currentPage = 0; updateScreenLanguage(); }
    else if (data == "PAGE_1") { currentPage = 1; updateScreenLanguage(); pushWeatherUI(); }
    else if (data == "PAGE_2") { currentPage = 2; updateScreenLanguage(); }
    else if (data == "PAGE_3") { currentPage = 3; updateClockUI(); }
    else if (data == "PAGE_4") { currentPage = 4; updateScreenLanguage(); }    
    
    // --- Screen Brightness Controls ---
    else if (data == "BRIGHT_UP") { 
      currentBrightness = min(100, currentBrightness + 10); // Increase by 10, cap at 100
      if(currentPage == 2) sendNumToNextion("page2.txtBrightVal", currentBrightness); 
      saveSettings();
    }
    else if (data == "BRIGHT_DOWN") { 
      currentBrightness = max(0, currentBrightness - 10);   // Decrease by 10, floor at 0
      if(currentPage == 2) sendNumToNextion("page2.txtBrightVal", currentBrightness);
      saveSettings();
    }
    else if (data == "BRIGHT_MAX") { 
      currentBrightness = 100; 
      if(currentPage == 2) sendNumToNextion("page2.txtBrightVal", 100);
      saveSettings();
    }
    else if (data == "BRIGHT_MIN") { 
      currentBrightness = 10; 
      if(currentPage == 2) sendNumToNextion("page2.txtBrightVal", 10);
      saveSettings();
    }
    else if (data == "BRIGHT_AUTO_TOGGLE") { isAutoBright = !isAutoBright; } // Toggle LDR usage

    // --- Audio Volume Controls (UI overrides Potentiometer temporarily) ---
    else if (data == "VOL_UP") { 
      currentVolume = min(100, currentVolume + 10);
      if(currentPage == 2) sendTextToNextion("page2.txtVolVal", String(currentVolume)); 
      preferences.putInt("vol", currentVolume);
    }
    else if (data == "VOL_DOWN") { 
      currentVolume = max(0, currentVolume - 10); 
      if(currentPage == 2) sendTextToNextion("page2.txtVolVal", String(currentVolume));
      preferences.putInt("vol", currentVolume);
    }
    else if (data == "VOL_MAX") { 
      currentVolume = 100; 
      if(currentPage == 2) sendTextToNextion("page2.txtVolVal", "100");
      preferences.putInt("vol", currentVolume);
    }
    else if (data == "VOL_MUTE") { 
      currentVolume = 0; 
      if(currentPage == 2) sendTextToNextion("page2.txtVolVal", "0");
      preferences.putInt("vol", currentVolume);
    }

    // --- LED Strip Power and Modes ---
    else if (data == "LIGHT_POWER") { 
      isLightOn = !isLightOn; // Toggle state
      if(!isLightOn) { fill_solid(leds, NUM_LEDS, CRGB::Black); FastLED.show(); } // Turn off
      else { fill_solid(leds, NUM_LEDS, currentColor); FastLED.show(); }          // Turn on to last color
      saveSettings();
    }
    else if (data == "LIGHT_MODE_STATIC") { currentLightMode = 0; saveSettings(); }
    else if (data == "LIGHT_MODE_BREATH") { currentLightMode = 1; saveSettings(); }
    else if (data == "LIGHT_MODE_SNAKE")  { currentLightMode = 2; saveSettings(); }
    
    // --- LED Brightness Controls ---
    else if (data == "LIGHT_BRT_UP") { 
      ledBrightness = min(255, ledBrightness + 25); // Increase internal FastLED brightness
      FastLED.setBrightness(ledBrightness); FastLED.show(); 
      if(currentPage == 4) sendTextToNextion("page4.txtBrightVal", String(map(ledBrightness, 0, 255, 0, 100))); 
      saveSettings();
    }
    else if (data == "LIGHT_BRT_DOWN") { 
      ledBrightness = max(10, ledBrightness - 25);
      FastLED.setBrightness(ledBrightness); FastLED.show(); 
      if(currentPage == 4) sendTextToNextion("page4.txtBrightVal", String(map(ledBrightness, 0, 255, 0, 100))); 
      saveSettings();
    }
    else if (data == "LIGHT_BRT_MAX") { 
      ledBrightness = 255;
      FastLED.setBrightness(ledBrightness); FastLED.show(); 
      if(currentPage == 4) sendTextToNextion("page4.txtBrightVal", "100"); 
      saveSettings();
    }
    else if (data == "LIGHT_BRT_MIN") { 
      ledBrightness = 10;
      FastLED.setBrightness(ledBrightness); FastLED.show(); 
      if(currentPage == 4) sendTextToNextion("page4.txtBrightVal", "4"); 
      saveSettings();
    }

    // --- LED Animation Speed Controls ---
    else if (data == "LIGHT_SPD_UP") { 
      ledSpeed = min(100, ledSpeed + 10);
      if(currentPage == 4) sendTextToNextion("page4.txtSpeedVal", String(ledSpeed)); 
      saveSettings();
    }
    else if (data == "LIGHT_SPD_DOWN") { 
      ledSpeed = max(10, ledSpeed - 10);
      if(currentPage == 4) sendTextToNextion("page4.txtSpeedVal", String(ledSpeed)); 
      saveSettings();
    }
    else if (data == "LIGHT_SPD_MAX") { 
      ledSpeed = 100;
      if(currentPage == 4) sendTextToNextion("page4.txtSpeedVal", "100"); 
      saveSettings();
    }
    else if (data == "LIGHT_SPD_MIN") { 
      ledSpeed = 10;
      if(currentPage == 4) sendTextToNextion("page4.txtSpeedVal", "10"); 
      saveSettings();
    }
    
    // Catch-all for Nextion Color Palette clicks
    else if (data.startsWith("COLOR_")) { 
      currentLightMode = 0; // Override animations if a solid color is selected
      handleColorCommand(data);
    }

    // --- Stopwatch Controls ---
    else if (data == "SW_TOGGLE") {
      isStopwatchRunning = !isStopwatchRunning;
      if (isStopwatchRunning) lastStopwatchTick = millis(); // Initialize tick timer
      if (currentVolume > 0) tone(AUDIO_PIN, isStopwatchRunning ? 3500 : 1500, 150); // Feedback beep
    }
    else if (data == "SW_RESET") {
      isStopwatchRunning = false;
      stopwatchElapsedSeconds = 0; // Clear accumulator
      updateClockUI(); 
      if (currentVolume > 0) tone(AUDIO_PIN, 1000, 100);
    }

    // --- Alarm Configuration ---
    else if (data.startsWith("ALM_SLOT_")) {
      currentAlarmSlot = data.substring(9).toInt() - 1; // Extract slot number (1-5) and 0-index it
      inputAlarmBuffer = alarms[currentAlarmSlot];      // Load slot into working buffer
      updateClockUI();
      if (currentVolume > 0) tone(AUDIO_PIN, 2000, 50);
    }
    else if (data == "ALM_CONFIRM") { 
      alarms[currentAlarmSlot] = inputAlarmBuffer;      // Save buffer to active slot
      preferences.putString(("alm" + String(currentAlarmSlot)).c_str(), inputAlarmBuffer); // Save to NVS
      updateClockUI();
      if (currentVolume > 0) tone(AUDIO_PIN, 3000, 150);
    }
    else if (data == "ALM_CLEAR") {
      inputAlarmBuffer = "0000";         // Reset buffer
      alarms[currentAlarmSlot] = "0000"; // Reset active slot
      preferences.putString(("alm" + String(currentAlarmSlot)).c_str(), "0000"); // Save to NVS
      updateClockUI();
      if (currentVolume > 0) tone(AUDIO_PIN, 1000, 100);
    }
    else if (data.startsWith("ALM_KEY_")) { 
      // Shift buffer left and append new digit (e.g., "0012" + '5' -> "0125")
      inputAlarmBuffer = inputAlarmBuffer.substring(1) + data.substring(8);
      updateClockUI();
    }

    // --- Timer Configuration ---
    else if (data.startsWith("TMR_KEY_")) { 
      // Shift buffer left and append new digit (e.g., "000012" + '5' -> "000125")
      inputTimerBuffer = inputTimerBuffer.substring(1) + data.substring(8);
      updateClockUI(); 
      if (currentVolume > 0) tone(AUDIO_PIN, 2000, 50);
    }
    else if (data.startsWith("TMR_QUICK_")) {
      int mins = data.substring(10).toInt(); // Extract minutes from command
      char buf[7];
      sprintf(buf, "%02d%02d00", mins / 60, mins % 60); // Format into HHMMSS buffer
      inputTimerBuffer = String(buf); 
      updateClockUI();
      if (currentVolume > 0) tone(AUDIO_PIN, 2500, 80);
    }
    else if (data == "TMR_CLEAR") { 
      inputTimerBuffer = "000000"; // Reset buffer
      isTimerRunning = false;      // Halt execution
      timerRemainingSeconds = 0;   // Reset logic
      updateClockUI(); 
      if (currentVolume > 0) tone(AUDIO_PIN, 1000, 100);
    }
    else if (data == "TMR_CONFIRM") {
      // Parse HHMMSS buffer into absolute seconds
      int h = inputTimerBuffer.substring(0, 2).toInt();
      int m = inputTimerBuffer.substring(2, 4).toInt();
      int s = inputTimerBuffer.substring(4, 6).toInt();
      timerRemainingSeconds = (h * 3600) + (m * 60) + s;
      isTimerRunning = false; // Await toggle command
      if (currentVolume > 0) tone(AUDIO_PIN, 3000, 150);
    }
    else if (data == "TMR_TOGGLE") {
      if (timerRemainingSeconds > 0) { // Only start if time is set
        isTimerRunning = !isTimerRunning;
        if (isTimerRunning) lastTimerTick = millis(); 
        if (currentVolume > 0) {
            if (isTimerRunning) tone(AUDIO_PIN, 3500, 150); // High pitch for start
            else tone(AUDIO_PIN, 1500, 150);                // Low pitch for pause
        }
      }
    }
  }
}

// ==========================================
// 8. TIME & NETWORK FUNCTIONS
// ==========================================

// Fetches the internal ESP32 RTC time and formats it as HH:MM
String getCurrentTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo,10)) return "--:--"; // Return fallback if NTP sync failed
  char timeStringBuff[10];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M", &timeinfo);
  return String(timeStringBuff);
}

// Fetches the internal ESP32 RTC date and formats it
String getCurrentDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return "--- --, ----";
  char dateStringBuff[40]; 
  strftime(dateStringBuff, sizeof(dateStringBuff), "%A, %b %d, %Y", &timeinfo); // E.g., "Monday, Jan 01, 2026"
  return String(dateStringBuff);
}

// Queries IP-API to determine coordinates based on the public IP address
void updateLocationFromWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http; 
    http.setTimeout(10000); // 10-second timeout
    http.begin("http://ip-api.com/json/"); 
    int httpCode = http.GET(); // Execute request
    
    if (httpCode == 200) { // HTTP OK
      String payload = http.getString();
      JsonDocument doc; 
      deserializeJson(doc, payload); // Parse response
      currentLat = doc["lat"];       // Extract latitude
      currentLon = doc["lon"];       // Extract longitude
    }
    http.end(); // Free resources
  }
}

// Queries OpenWeatherMap using the previously fetched coordinates
void fetchExternalWeather() {
  if (WiFi.status() == WL_CONNECTED && currentLat != 0.0) { // Ensure WiFi and Location exist
    HTTPClient http;
    http.setTimeout(10000);
    // Construct API URL with location, key, metric units, and selected language
    String url = "http://api.openweathermap.org/data/2.5/weather?lat=" + String(currentLat, 4) + "&lon=" + String(currentLon, 4) + "&appid=" + openWeatherApiKey + "&units=metric&lang=" + apiLangCodes[currentLang];
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      JsonDocument doc; 
      deserializeJson(doc, payload); // Parse JSON data
      
      if (!doc.isNull()) {
        // Extract basic metrics
        float temp = doc["main"]["temp"];
        float feelsLike = doc["main"]["feels_like"]; 
        float humidity = doc["main"]["humidity"];
        float windSpeed = doc["wind"]["speed"]; 
        
        int windSpeedKmh = round(windSpeed * 3.6); // Convert m/s to km/h
        String mainCond = doc["weather"][0]["main"]; 
        String weatherDesc = doc["weather"][0]["description"];
        if (weatherDesc.length() > 0) weatherDesc[0] = toupper(weatherDesc[0]); // Capitalize first letter

        // Custom Logic: Determine if conditions are suitable for running
        String runStatus = "Fair";
        if (temp >= 15 && temp <= 25 && mainCond != "Rain" && mainCond != "Snow") {
            runStatus = "Good";
        } else if (temp < 5 || temp > 32 || mainCond == "Rain" || mainCond == "Thunderstorm") {
            runStatus = "Poor";
        }

        // Format strings for UI push
        outTempStr = String(temp, 0) + " C";
        outCondStr = mainCond;
        outMinMaxStr = "Feels like " + String(feelsLike, 0);
        outWindStr = "Wind: " + String(windSpeedKmh) + " km/h";
        outRunStr = "Running: " + runStatus;
        outDescStr = weatherDesc;
        outHumStr = "Humidity: " + String(humidity, 0) + "%";
        
        if (currentPage == 1) { pushWeatherUI(); } // Update UI immediately if on Weather page
      }
    } 
    http.end();
  } else if (currentLat == 0.0) {
    updateLocationFromWiFi(); // Attempt to get location if it's missing
  }
}

// ==========================================
// 9. HARDWARE HANDLERS & ALARMS
// ==========================================

// Outputs an oscillating emergency siren sound to the Buzzer via PWM
void triggerSiren() {
  if (currentVolume == 0) return; // Respect global mute

  int dutyCycle = map(currentVolume, 0, 100, 0, 127); // Scale volume to 50% max duty cycle (0-255)

  for (int i = 0; i < 4; i++) {
    ledcSetup(0, 2500, 8);       // Channel 0, 2500Hz frequency, 8-bit resolution
    ledcAttachPin(AUDIO_PIN, 0); // Attach buzzer to PWM channel
    ledcWrite(0, dutyCycle);     // Play high tone
    delay(300);

    ledcSetup(0, 2000, 8);       // Change frequency to 2000Hz
    ledcWrite(0, dutyCycle);     // Play low tone
    delay(300);
  }
  
  ledcWrite(0, 0);               // Stop audio
  ledcDetachPin(AUDIO_PIN);      // Release pin
}

// Process Ultrasonic sensor data for hand hovering and holding gestures
void handleAdvancedGestures() {
  if (millis() - lastPingTime < 50) return; // Rate limit sensor to prevent echo interference
  lastPingTime = millis();

  // Send a 10us HIGH pulse to trigger pin
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Read echo time in microseconds and convert to cm
  long duration = pulseIn(ECHO_PIN, HIGH, 3000); // 3ms timeout (~50cm max)
  int distance = (duration == 0) ? 999 : (duration * 0.034 / 2); 
  
  if (distance > 2 && distance < 15) { // Hand detected between 2cm and 15cm
    lastValidHandTime = millis();
    
    if (!isHandPresent) { // First detection event
      isHandPresent = true; 
      handPresentStartTime = millis(); 
      holdActionTriggered = false;
    } 
    // Hold Gesture: Hand kept in place for >2 seconds
    else if (millis() - handPresentStartTime > 2000 && !holdActionTriggered) {
         isLightOn = !isLightOn; // Toggle main lights
         if (isLightOn) fill_solid(leds, NUM_LEDS, currentColor);
         else fill_solid(leds, NUM_LEDS, CRGB::Black);
         FastLED.show(); 
         saveSettings();
         holdActionTriggered = true; // Prevent re-triggering until hand is removed
    }
  } 
  else { // Hand removed
    if (isHandPresent && (millis() - lastValidHandTime > 300)) { // 300ms debounce
       unsigned long presentDuration = lastValidHandTime - handPresentStartTime;
       
       // Swipe/Hover Gesture: Hand present for 0.1s to 1s
       if (presentDuration > 100 && presentDuration < 1000 && !holdActionTriggered) {
          // Cycle to the next quick color
          currentColorIndex = (currentColorIndex + 1) % 6;
          currentColor = quickColors[currentColorIndex];
          if (isLightOn) { fill_solid(leds, NUM_LEDS, currentColor); FastLED.show(); }
          saveSettings();
       }
       isHandPresent = false; // Reset state machine
    }
  }
}

// Uses PIR motion sensor and LDR to manage screen power and brightness
void handleSmartScreen() {
  if (digitalRead(PIR_PIN) == HIGH) { // Motion detected
    lastMotionTime = millis();
    if (!isScreenAwake) { 
      sendToNextion("sleep=0"); // Wake Nextion display
      isScreenAwake = true;
      if (wasLightOnBeforeSleep) { // Restore LED state if they were on prior
        isLightOn = true;
        if (currentLightMode == 0) { 
          fill_solid(leds, NUM_LEDS, currentColor);
          FastLED.show();
        }
        wasLightOnBeforeSleep = false;
      }
    }
  } else { // No motion
    if (isScreenAwake && (millis() - lastMotionTime > screenTimeout)) { // Timeout reached
      sendToNextion("sleep=1"); // Put Nextion display to sleep
      isScreenAwake = false;
      wasLightOnBeforeSleep = isLightOn; // Remember LED state
      if (isLightOn) {
        isLightOn = false; // Turn off LEDs to save power
        fill_solid(leds, NUM_LEDS, CRGB::Black); 
        FastLED.show();
      }
    }
  }

  // Handle active screen brightness
  if (isScreenAwake) {
    if (isAutoBright) {
      // Basic 2-state LDR handling: Dim if dark, max if bright
      if (digitalRead(LDR_PIN) == HIGH) { sendToNextion("dim=20"); sendNumToNextion("txtBrightVal", 20); } 
      else { sendToNextion("dim=100"); sendNumToNextion("txtBrightVal", 100); } 
    } else {
      // Enforce manual brightness
      sendToNextion("dim=" + String(currentBrightness));
    }
  }
}

// Non-blocking processor for FastLED animations
void handleLEDModes() {
  if (!isLightOn) return; // Do nothing if lights are off

  if (currentLightMode == 1) { // Mode: Breathing
    float speedFactor = map(ledSpeed, 10, 100, 4000, 800); // Inverse map speed to period
    // Math logic for a smooth, exponential-sinusoidal breathing curve
    float breath = (exp(sin(millis() / speedFactor * PI)) - 0.36787944) * 108.0;
    FastLED.setBrightness(map(breath, 0, 255, 10, ledBrightness)); // Map curve to set brightness
    FastLED.show();
  } 
  else if (currentLightMode == 2) { // Mode: Snake Chaser
    int speedDelay = map(ledSpeed, 10, 100, 80, 15); // Inverse map to delay ms
    static unsigned long lastSnakeUpdate = 0;
    static int snakeHead = 0; // Tracks the leading LED
    
    if (millis() - lastSnakeUpdate > speedDelay) {
      fadeToBlackBy(leds, NUM_LEDS, 50); // Fade out previous LEDs to create a tail
      leds[snakeHead] = currentColor;    // Light up current head
      FastLED.setBrightness(ledBrightness);
      FastLED.show();
      
      snakeHead++; // Move head forward
      if (snakeHead >= NUM_LEDS) snakeHead = 0; // Wrap around
      lastSnakeUpdate = millis();
    }
  }
  else if (currentLightMode == 3) { // Mode: Moving Rainbow
    fill_rainbow(leds, NUM_LEDS, 0, 255 / NUM_LEDS); // Map full hue spectrum across strip
    FastLED.setBrightness(ledBrightness); FastLED.show();
  }
  // Modes 4, 5, 6: Static Gradients
  else if (currentLightMode == 4) {
    fill_gradient_RGB(leds, NUM_LEDS, CRGB(75, 0, 130), CRGB(0, 0, 255)); // Indigo to Blue
    FastLED.setBrightness(ledBrightness); FastLED.show();
  }
  else if (currentLightMode == 5) {
    fill_gradient_RGB(leds, NUM_LEDS, CRGB(255, 105, 180), CRGB(255, 165, 0)); // Hot Pink to Orange
    FastLED.setBrightness(ledBrightness); FastLED.show();
  }
  else if (currentLightMode == 6) {
    fill_gradient_RGB(leds, NUM_LEDS, CRGB(0, 255, 0), CRGB(0, 128, 128)); // Green to Teal
    FastLED.setBrightness(ledBrightness); FastLED.show();
  }
  else { // Mode 0: Static Solid Color
    FastLED.setBrightness(ledBrightness); // Enforce brightness (in case user just switched back)
  }
}

// Reads hardware potentiometer and applies EMA filtering for volume
void handleVolumeControl() {
  static int lastPotValue = -1;
  static float smoothedPotValue = 0; 
  
  int rawPotValue = analogRead(POT_PIN); // Read 12-bit ADC (0-4095)
  if (smoothedPotValue == 0) smoothedPotValue = rawPotValue; // Initialize filter
  
  // Exponential Moving Average filter to reduce analog noise/jitter
  smoothedPotValue = (0.1 * rawPotValue) + (0.9 * smoothedPotValue);
  
  int mappedVolume = map((int)smoothedPotValue, 0, 4095, 0, 100); // Scale to percentage
  
  // Only register change if it jumps by >3% (deadband to prevent flickering)
  if (abs(mappedVolume - lastPotValue) >= 3) {
    currentVolume = mappedVolume;
    
    if(currentPage == 2) { // Update UI if on settings page
      sendTextToNextion("page2.txtVolVal", String(currentVolume)); 
    }
    lastPotValue = currentVolume;
    
    pendingVolSave = true; // Queue save
    lastVolChangeTime = millis();
  }

  // NVS Save Debouncing: Only commit to flash 5 seconds after user STOPS turning knob
  // This drastically extends the life of the ESP32's flash memory
  if (pendingVolSave && (millis() - lastVolChangeTime > 5000)) {
    preferences.putInt("vol", currentVolume);
    pendingVolSave = false;
  }
}

// Increments stopwatch clock once per second
void handleStopwatch() {
  if (isStopwatchRunning) {
    if (millis() - lastStopwatchTick >= 1000) {
      lastStopwatchTick += 1000;   // Keep rigid sync to avoid drift
      stopwatchElapsedSeconds++;
      updateClockUI();             // Refresh display
    }
  }
}

// Compares current system time against all set alarms
void checkAlarms(String currentTime) {
  // Format current HH:MM to HHMM to match alarm format
  String currentFormatted = currentTime.substring(0, 2) + currentTime.substring(3, 5);
  
  // Bail out early if we already triggered an alarm this minute
  if (currentFormatted == lastTriggeredTime) return;
  
  for (int i = 0; i < 5; i++) {
    if (alarms[i] != "0000" && alarms[i] == currentFormatted) { // Found match
      lastTriggeredTime = currentFormatted; // Lockout further triggers this minute
      sendToNextion("page 3"); // Force UI to the Clock page
      triggerSiren();          // Sound alarm
      break;
    }
  }
}

// Decrements timer clock once per second and handles completion
void handleCountdown() {
  if (isTimerRunning && timerRemainingSeconds > 0) {
    if (millis() - lastTimerTick >= 1000) { 
      lastTimerTick += 1000;
      timerRemainingSeconds--;
      
      // Calculate individual time units
      int h = timerRemainingSeconds / 3600;
      int m = (timerRemainingSeconds % 3600) / 60;
      int s = timerRemainingSeconds % 60;
      
      // Rebuild the input buffer for the UI formatting function
      char buf[7];
      sprintf(buf, "%02d%02d%02d", h, m, s);
      inputTimerBuffer = String(buf);
      
      updateClockUI(); // Push new time to display
      
      // Handle completion event
      if (timerRemainingSeconds <= 0) {
        isTimerRunning = false;
        triggerSiren(); // Alert user
      }
    }
  }
}

// ==========================================
// 10. SETUP & MAIN LOOP
// ==========================================

void setup() {
  // 1. Initialize Serial Interfaces
  Serial.begin(115200);                             // Debug Console
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);    // Nextion HMI UART
  Serial2.setTimeout(50);                           // Quick timeout for snappy reads
  
  // 2. Configure Hardware Pins
  pinMode(LDR_PIN, INPUT); pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT);
  dht.begin(); // Start temperature/humidity sensor
  
  // 3. Mount File System and Load Settings
  preferences.begin("workspace", false); // Open "workspace" namespace in read/write mode
  currentLang = preferences.getInt("lang", 0);
  currentBrightness = preferences.getInt("bright", 80);
  currentVolume = preferences.getInt("vol", 60);
  isLightOn = preferences.getBool("lOn", false);
  ledBrightness = preferences.getInt("lBrt", 100);
  ledSpeed = preferences.getInt("lSpd", 50);
  currentLightMode = preferences.getInt("lMode", 0);
  currentColorIndex = preferences.getInt("cIdx", 0);
  currentColor = quickColors[currentColorIndex % 6]; 

  for (int i = 0; i < 5; i++) { // Load all 5 alarm slots
    alarms[i] = preferences.getString(("alm" + String(i)).c_str(), "0000");
  }
  inputAlarmBuffer = alarms[0]; // Set UI buffer to first slot
  
  // 4. Initialize WiFi Connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int wifiRetries = 0;
  // Block setup for up to 10 seconds waiting for connection
  while (WiFi.status() != WL_CONNECTED && wifiRetries < 20) { 
    delay(500);
    wifiRetries++;
  }
  
  // 5. Initialize FastLED Strip
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(ledBrightness);
  if (isLightOn) { fill_solid(leds, NUM_LEDS, currentColor); }
  else { fill_solid(leds, NUM_LEDS, CRGB::Black); }
  FastLED.show();

  // 6. Post-Network Sync
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); // Sync RTC with NTP
  updateLocationFromWiFi();  // Get IP-based location
  fetchExternalWeather();    // Pull initial weather state
  updateScreenLanguage();    // Push initial language/UI state
}

void loop() {
  // 1. Maintain Network Connection
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiRetryTime > 30000) { // Retry every 30 seconds
      WiFi.disconnect();
      WiFi.reconnect();
      lastWifiRetryTime = millis();
    }
  }

  // 2. Poll High-Frequency Handlers
  listenToNextion();        // Check for UI touch events
  handleAdvancedGestures(); // Check for ultrasonic proximity
  handleSmartScreen();      // Check motion timeout / auto-brightness
  handleVolumeControl();    // Check potentiometer
  handleLEDModes();         // Step FastLED animations
  handleCountdown();        // Step timer logic
  handleStopwatch();        // Step stopwatch logic

  // 3. Low-Frequency Tasks (Weather)
  if (millis() - lastWeatherUpdate > 900000) { // Check API every 15 minutes (900,000ms)
    fetchExternalWeather();
    lastWeatherUpdate = millis();
  }

  // 4. Medium-Frequency Tasks (UI Data Refresh)
  if (millis() - lastDisplayUpdate > 5000) { // Update sensor UI every 5 seconds
    String currentTime = getCurrentTime();
    checkAlarms(currentTime); // Cross-reference alarms with new time
    
    // Read local environment sensors
    float inTemp = dht.readTemperature();
    float inHum = dht.readHumidity();
    int airQuality = analogRead(MQ135_PIN); 
    
    // Push updates to the active page
    if (currentPage == 0) { // Home Page Updates
      sendTextToNextion("txtTime", currentTime);
      sendTextToNextion("txtDate", getCurrentDate());
    }
    
    if (currentPage == 1) { // Environment Page Updates
      if (!isnan(inTemp)) { // Ensure DHT returned valid data
        sendTextToNextion("txtInTemp", String(inTemp, 1) + " C");
        sendTextToNextion("txtInHum", String(inHum, 0) + "%");
        
        // Contextual strings based on metrics
        if(inTemp > 28) sendTextToNextion("txtInStatus", "Warm");
        else sendTextToNextion("txtInStatus", "Comfortable");

        float inFeel = dht.computeHeatIndex(inTemp, inHum, false); // Calc heat index (Celsius)
        sendTextToNextion("txtInFeel", "Feels like " + String(inFeel, 0)); 
        
        if(inHum > 60) sendTextToNextion("txtInHumStat", "High");
        else if (inHum < 30) sendTextToNextion("txtInHumStat", "Dry");
        else sendTextToNextion("txtInHumStat", "Good");
      }
      
      // Update Air Quality UI
      sendTextToNextion("txtInPPM", String(airQuality)); 
      if (airQuality > 800) { // High threshold trigger
        sendTextToNextion("txtInAirStat", "Warning: High"); 
        sendTextToNextion("txtInGas", "Smoke Detected!");
      } else { 
        sendTextToNextion("txtInAirStat", "Fresh"); 
        sendTextToNextion("txtInGas", "Safe");
      }
    }
    
    if (currentPage == 2) { // Settings Page Updates
      if (WiFi.status() == WL_CONNECTED) {
        sendTextToNextion("page2.txtWifiName", WiFi.SSID()); // Display current network name
      } else {
        sendTextToNextion("page2.txtWifiName", "Disconnected");
      }
    }
    
    lastDisplayUpdate = millis(); // Reset update clock
  }
}