# 💧 Smart Jal Rakshak
### An Intelligent Flood & Water Management System for Odisha

> *"Smart Jal Rakshak uses real-time water sensing and automated control to turn flood management from reaction into prevention."*

---

## 1. Synopsis

Smart Jal Rakshak is an intelligent, sensor-based water and flood management system designed to help prevent and manage flood situations in Odisha. The model uses water-depth, flow and rain sensors to continuously monitor rising water levels and flow conditions.

When the water level reaches a critical point, the system gives an early warning and automatically activates a sluice gate and pump to regulate water flow. The project also shows how villages, drainage channels, ponds, embankments and flood-prone areas can be connected through a smart monitoring system.

The main aim is to **detect, divert and regulate excess water**, reduce flood damage, protect communities and promote efficient water management.

## 2. Idea / Innovation

| Step | Action |
|---|---|
| 🌊 **Detect** | Water-depth, flow and rain sensors monitor changing water conditions |
| 📡 **Monitor** | A microcontroller continuously processes the sensor data |
| 🚨 **Alert** | Early warning when water rises beyond a safe level |
| 🚪 **Control** | Automated sluice gate regulates the movement of water |
| 💧 **Divert** | Excess water is directed to suitable storage/drainage areas |
| 🏘️ **Protect** | Early action protects villages, roads, farmland and people |

## 3. Model / Design

- Flood-prone village area
- Water reservoir / river channel
- Water-depth (ultrasonic) sensor
- Flow sensor
- Rain sensor
- Microcontroller / control unit (Seeed Studio XIAO ESP32-C6)
- Automatic sluice gate (servo)
- Water pump (relay-switched)
- 16x4 I2C LCD for local status
- Drainage / diversion channel
- Pebbles, coarse sand and gravel filtration section
- Safe-zone / storage area

## 4. Expected Output

When the water level rises, the sensor detects the change and sends data to the control unit. The system raises an alert and operates the gate and pump, helping divert or regulate excess water. Status is shown on the LCD and in the Blynk mobile/web dashboard.

## 5. Key Objective

To develop a **low-cost, intelligent and automated** system for early flood detection, water-flow regulation and efficient water management, particularly for flood-prone regions of Odisha.

## 6. Real-Life Application

The concept can be adapted for villages, urban drainage systems, rivers, dams, reservoirs, agricultural fields and flood-prone communities to support early warning and better water management.

---

## 🔌 Hardware & Pin Configuration

Board: **Seeed Studio XIAO ESP32-C6**

| Component | XIAO Pin | GPIO | Function |
|---|---|---|---|
| JSN-SR04T Trig | D0 | GPIO0 | Ultrasonic trigger |
| JSN-SR04T Echo | D1 | GPIO1 | Ultrasonic echo (**use a voltage divider to 3.3 V**) |
| Rain sensor (AO) | D2 | GPIO2 | Analog rain / water input |
| Flow meter signal | D3 | GPIO21 | Pulse counter (interrupt) |
| LCD SDA | D4 | GPIO22 | I2C data |
| LCD SCL | D5 | GPIO23 | I2C clock |
| Servo (sluice gate) | D6 | GPIO16 | PWM gate control |
| Relay (water pump) | D7 | GPIO17 | Pump switch |

> The code uses the `D0`–`D7` labels from the XIAO core, so the pins above map automatically.

**Power notes:** power the servo and pump from a separate supply and join all grounds with the XIAO. The JSN-SR04T cannot measure closer than about 20 cm, so mount it above the highest expected water level.

## 🧠 How It Works (AUTO mode)

| Water level | State | Action |
|---|---|---|
| ≤ 45% | SAFE | Gate closed, pump off |
| ≥ 60% | WARNING | Gate half open (45°), alert |
| ≥ 80% | DANGER | Gate fully open (90°), pump ON to divert |
| ≥ 90% | CRITICAL | Same actions with critical alert |

Operating modes (set from Blynk, V7):
- **AUTO**: the system controls gate and pump from sensor data.
- **MANUAL**: the operator controls gate and pump; safety interlocks still apply.
- **AI**: advice only. Gemini suggests actions through a secure HTTPS backend; it **cannot** operate hardware.

**Built-in safety:**
- Pump and gate are OFF/closed at boot; no cloud state is synced to the actuators on start-up.
- Dry-run protection (pump off below 10% level).
- No-flow detection: if the pump runs for 15 s without flow pulses, it is switched off and the fault stays latched until cleared.
- Sensor fault: pump off, gate holds its last position.
- Local control keeps working if Wi-Fi or Blynk goes offline.

## 📱 Blynk Virtual Pins

| Pin | Purpose |
|---|---|
| V0 | Distance (cm) |
| V1 | Water level (%) |
| V2 | Flow (L/min) |
| V3 | Total volume (L) |
| V4 | Rain detected (0/1) |
| V5 | Pump command / state |
| V6 | Gate angle (0–90) |
| V7 | Mode (0 AUTO, 1 MANUAL, 2 AI) |
| V8 | AI recommendation text |
| V9 | Alert / status text |
| V10 | Button: request AI analysis |
| V11 | Button: clear latched fault |
| V12 | Rain intensity (%) |
| V13 | Flood state (0 SAFE, 1 WARNING, 2 DANGER, 3 CRITICAL, 4 FAULT) |

## 🛠️ Software Setup

1. Install the **Arduino IDE** and add the ESP32 board package (**esp32 by Espressif**, 3.x). Select the board **XIAO_ESP32C6**.
2. Install libraries from the Library Manager: **Blynk**, **ArduinoJson**, **LiquidCrystal_I2C**, **ESP32Servo**.
3. Open `Smart_Jal_Rakshak.ino` and edit:
   ```cpp
   #define BLYNK_TEMPLATE_ID   "TMPLxxxxxxxx"
   #define BLYNK_AUTH_TOKEN    "YOUR_BLYNK_DEVICE_AUTH_TOKEN"
   const char WIFI_SSID[]     = "YOUR_WIFI_NAME";
   const char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";
   ```
4. Calibrate in the config section: `SENSOR_HEIGHT_CM`, `FLOW_PULSES_PER_LITER`, `RAIN_ADC_DRY` / `RAIN_ADC_WET`, and the level thresholds. If the LCD stays blank, change `LCD_ADDRESS` from `0x27` to `0x3F`.
5. **Test with the pump disconnected and the servo arm unloaded first**, then upload.

### Optional: Gemini AI advice
Deploy a small HTTPS backend that holds your Gemini API key and exposes `/api/analyze`. Set `BACKEND_URL`, `DEVICE_API_TOKEN` and `BACKEND_ROOT_CA` in the sketch. **Never put the Gemini API key in the firmware.** Leave the defaults to disable this feature.

## ⚠️ Security

Do not commit real Wi-Fi passwords, Blynk tokens or API tokens to a public repository. Use placeholders and regenerate any token that was ever published.

## 📂 Repository Contents

```
Smart_Jal_Rakshak/
├── Smart_Jal_Rakshak.ino   # Main firmware
└── README.md               # This file
```

## 📄 License

Choose a license for your project (for example MIT) and add a `LICENSE` file.

---

*Made for flood-prone communities of Odisha 🌊➡️🛡️*
