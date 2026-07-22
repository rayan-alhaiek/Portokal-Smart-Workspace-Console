# 🍊 Portokal: ESP32-Based Smart Workspace Console

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![University](https://img.shields.io/badge/University-Bah%C3%A7e%C5%9Fehir%20University-blue)](https://bau.edu.tr/)
[![Course](https://img.shields.io/badge/Course-CMP3010%20Embedded%20Systems-orange)](docs/CMP3010_Portokal_Report.pdf)

**Portokal** is an all-in-one localized desktop workspace assistant built on the ESP32 microcontroller platform. While commercial smart clocks rely heavily on external cloud weather data, Portokal bridges this gap by offering real-time local indoor air quality and climate monitoring alongside daily productivity features, touchless gesture control, an interactive Nextion touch interface, and live REST API integrations.

---

## 📁 Repository Structure

```text
Portokal-Smart-Workspace-Console/
├── Project video/
│   └── Project video.mp4         # Video demonstration of the console
├── docs/
│   ├── CMP3010_Portokal_Report.pdf  # Final academic engineering report
│   └── Porotakl_Presentation.pdf   # Presentation slides deck
├── firmware/
│   └── Portokal_ESP32.ino        # Core ESP32 C++/Arduino firmware
├── hardware/
│   ├── Proteus_Schematic.pdsprj  # Proteus circuit design project file
│   └── circuit_diagram.PDF       # High-resolution PDF circuit schematic
├── nextion-ui/
│   └── display_gui.HMI           # Nextion Editor GUI layout source file
├── LICENSE                       # Project license
└── README.md                     # Repository documentation

```

---

## ✨ Key Features

* **Localized Environmental Sensing:** Real-time monitoring of indoor air quality (MQ-135) and climate metrics (DHT11 temperature & humidity).


* **Interactive Touch HMI:** Integrated 7.0" Nextion display (NX8048T070) communicating via UART to offload graphic rendering from the processor.


* **Touchless Gesture Control:** Ultrasonic sensor (HC-SR04) gesture processing for proximity hover/hold light toggling and RGB color mode switching.


* **Smart Energy & Display Management:** PIR motion-sensor-activated screen sleep (30s timeout) and LDR-based ambient auto-brightness.


* **Live External Data Integration:** ESP32 Wi-Fi fetching NTP time synchronization and OpenWeatherMap REST API forecasts.


* **Audio & RGB Visual Feedback:** Dual WS2812B RGB light rings (static, breathing, snake, and gradient modes) paired with a PAM8403 audio amplifier & hardware volume control.


* **Multi-Tiered Power Architecture:** Step-down 12V to 5V via an LM2596 buck converter for high-draw peripherals, with regulated 3.3V sensor logic rails.



---

## 🔌 Pinout Mapping

| Peripheral / Module | Hardware Pin | Function / Description |
| --- | --- | --- |
| **Nextion 7.0" Display** | RX2 (`GPIO 16`) / TX2 (`GPIO 17`) | Hardware Serial 2 UART Communication

 |
| **DHT11 Sensor** | `GPIO 4` | Temperature & Humidity Data Input

 |
| **MQ-135 Air Quality** | `GPIO 35` | Analog Gas / Air Quality Input

 |
| **LDR Sensor** | `GPIO 32` | Ambient Light Reading

 |
| **PIR Motion Sensor** | `GPIO 27` | Occupancy / Motion Detection

 |
| **WS2812B LED Strip** | `GPIO 13` | FastLED Data Output (32 LEDs)

 |
| **HC-SR04 Ultrasonic** | `GPIO 5` (Trig) / `GPIO 18` (Echo) | Distance & Gesture Sensing

 |
| **Audio / Speaker** | `GPIO 25` | PWM Audio Signal Output

 |
| **Potentiometer** | `GPIO 34` | Hardware Volume Control Input

 |

---

## 🛠️ Software Dependencies & Libraries

* **Core Framework:** Arduino ESP32 Board Manager
* **Required Libraries:**
* `FastLED` - RGB LED animation control


* `ArduinoJson` - REST API JSON payload parsing


* `DHT sensor library` - Temperature & humidity sensor readings


* `Preferences` - ESP32 NVS non-volatile flash storage management


* `WiFi` & `HTTPClient` - Network connectivity & API communication





---

## 🚀 Getting Started

1. **Firmware Setup:**
* Open `firmware/Portokal_ESP32.ino` in the Arduino IDE.
* Install the required libraries listed above via the Library Manager.
* Configure your Wi-Fi credentials and OpenWeatherMap API key in the code:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
String openWeatherApiKey = "YOUR_OPENWEATHER_API_KEY";

```


* Select your ESP32 board and upload the code.


2. **Display Setup:**
* Open `nextion-ui/display_gui.HMI` in Nextion Editor.
* Compile and flash the `.tft` file to the 7.0" Nextion display via SD Card or USB-to-TTL module.


3. **Hardware Assembly:**
* Refer to `hardware/circuit_diagram.PDF` or open `hardware/Proteus_Schematic.pdsprj` for detailed wiring and power connections.





---

## 👥 Team & Acknowledgments

* **Course:** CMP3010 - Embedded Systems Programming


* **Institution:** Bahçeşehir University (BAU), Istanbul


* **Instructor:** Dr. Mahmut Ağan

**Project Team:**

* **Rayan Al Haiek** - Power Architecture, LM2596 Voltage Tuning, Nextion UI Design & Hardware Integration


* **Emir Alammari** - ESP32 Firmware Programming & Report/Presentation Documentation


* **Abdallah Moote** - Physical Construction, Assembly, Circuit Wiring, API Setup & Proteus Diagram



---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](https://www.google.com/search?q=LICENSE) file for details.
