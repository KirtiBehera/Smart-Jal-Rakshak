/*****************************************************************************
 * SMART JAL RAKSHAK - REAL-TIME EMBEDDED WEB DASHBOARD (DUAL PUMP + 180° SERVO)
 * Target Board : Seeed Studio XIAO ESP32-C6
 * IDE          : Arduino IDE
 * Made by Electrowander.online MIT licence (Kirti.behera)
 * Direct Web Server running on Port 80
 * URL Access   : http://smartjal.local  or  http://<ESP32_IP_ADDRESS>
 *****************************************************************************/

#define BLYNK_TEMPLATE_ID "TMPLxxxxxxxx"
#define BLYNK_TEMPLATE_NAME "Smart Jal Rakshak"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// Wi-Fi & Blynk Credentials
char auth[] = "h5xxxxxxxxxxxxxxxxxxxxx";
char ssid[] = "xxxxxx";
char pass[] = "xxxxxxx";

// Pin Definitions for Seeed Studio XIAO ESP32-C6
#define TRIG_PIN    0   // D0 / GPIO 0  (Ultrasonic Trigger)
#define ECHO_PIN    1   // D1 / GPIO 1  (Ultrasonic Echo - 3.3V divider)
#define RAIN_PIN    2   // D2 / GPIO 2  (Rain Sensor AO)
#define FLOW_PIN    21  // D3 / GPIO 21 (Flow Meter Interrupt)
#define SDA_PIN     22  // D4 / GPIO 22 (LCD SDA)
#define SCL_PIN     23  // D5 / GPIO 23 (LCD SCL)
#define SERVO_PIN   16  // D6 / GPIO 16 (Sluice Gate PWM)
#define RELAY_PIN1  17  // D7 / GPIO 17 (Pump Switch 1)
#define RELAY_PIN2  19  // D8 / GPIO 19 (Pump Switch 2)
#define BUZZER_PIN  20  // D9 / GPIO 20 (Buzzer)

// Hardware Settings
const bool RELAY_ACTIVE_LOW    = true;
const float TANK_MAX_DEPTH_CM  = 200.0; //Required
const int GATE_CLOSED          = 0;
const int GATE_HALF            = 90;
const int GATE_OPEN            = 180;

// Peripherals
WebServer server(80);
LiquidCrystal_I2C lcd(0x27, 16, 4);
Servo sluiceServo;
BlynkTimer timer;

// Sensor & System Variables
float waterLevelCm = 0.0;
int rainValue = 4095;
volatile int flowPulseCount = 0;
float flowRate = 0.0;
bool pump1State = false;
bool pump2State = false;
bool aiModeActive = true;
String systemAlert = "System Nominal";
String floodStateStr = "SAFE";
String previousFloodState = "";

// Buzzer & IP Scrolling Timing Variables
unsigned long lastWarnBuzzerTime = 0;
unsigned long lastScrollTime = 0;
int scrollIndex = 0;

void IRAM_ATTR countFlowPulses() {
  flowPulseCount++;
}

// Helper function to sound the buzzer using square-wave frequencies
void playBuzzerTone(int loops, int d1, int d2) {
  for (int cycle = 0; cycle < loops; cycle++) {
    for (int i = 0; i < 40; i++) {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(d1);
      digitalWrite(BUZZER_PIN, LOW);
      delay(d1);
    }
    if (d2 > 0) {
      for (int i = 0; i < 40; i++) {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(d2);
        digitalWrite(BUZZER_PIN, LOW);
        delay(d2);
      }
    }
  }
}

// Control individual or both pumps with dry-run protection
void setPumps(bool p1, bool p2) {
  if ((p1 || p2) && waterLevelCm < 5.0) {
    p1 = false;
    p2 = false;
    systemAlert = "Pumps blocked: Dry-run lock (< 5cm)";
  }
  
  pump1State = p1;
  pump2State = p2;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN1, p1 ? LOW : HIGH);
    digitalWrite(RELAY_PIN2, p2 ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN1, p1 ? HIGH : LOW);
    digitalWrite(RELAY_PIN2, p2 ? HIGH : LOW);
  }
}

float readWaterLevel() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return -1.0;
  float distanceCm = (duration * 0.0343) / 2.0;
  float level = TANK_MAX_DEPTH_CM - distanceCm;
  return (level < 0.0) ? 0.0 : level;
}

// ===========================================================================
// EMBEDDED HTML + CSS + JS DASHBOARD (Stored directly in ESP32 Flash Memory)
// ===========================================================================
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Jal Rakshak Dashboard | ESP32-C6 </title>
  <style>
    :root {
      --bg: #0b132b;
      --card-bg: rgba(28, 37, 65, 0.75);
      --accent-blue: #48cae4;
      --accent-green: #06d6a0;
      --accent-yellow: #ffd166;
      --accent-red: #ef476f;
      --text: #f0f4f8;
      --text-muted: #8d99ae;
      --border: rgba(255, 255, 255, 0.1);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 16px; min-height: 100vh; }
    .container { max-width: 950px; margin: 0 auto; }
    
    header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; padding-bottom: 15px; border-bottom: 1px solid var(--border); flex-wrap: wrap; gap: 10px; }
    .logo-group h1 { font-size: 1.5rem; font-weight: 700; color: #fff; display: flex; align-items: center; gap: 8px; }
    .logo-group p { font-size: 0.85rem; color: var(--accent-blue); }
    .badge { padding: 6px 14px; border-radius: 20px; font-weight: 600; font-size: 0.85rem; text-transform: uppercase; letter-spacing: 0.5px; }
    .badge-safe { background: rgba(6, 214, 160, 0.2); color: var(--accent-green); border: 1px solid var(--accent-green); }
    .badge-warn { background: rgba(255, 209, 102, 0.2); color: var(--accent-yellow); border: 1px solid var(--accent-yellow); }
    .badge-danger { background: rgba(239, 71, 111, 0.25); color: var(--accent-red); border: 1px solid var(--accent-red); animation: pulse 1.5s infinite; }
    @keyframes pulse { 0% { opacity: 0.8; } 50% { opacity: 1; transform: scale(1.02); } 100% { opacity: 0.8; } }

    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; margin-bottom: 20px; }
    .card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 16px; padding: 20px; backdrop-filter: blur(10px); box-shadow: 0 8px 24px rgba(0,0,0,0.3); }
    .card h3 { font-size: 0.9rem; color: var(--text-muted); text-transform: uppercase; margin-bottom: 12px; display: flex; justify-content: space-between; }
    
    .metric-value { font-size: 2.2rem; font-weight: 700; color: #fff; margin-bottom: 6px; }
    .metric-unit { font-size: 1rem; color: var(--text-muted); font-weight: 400; }
    .metric-sub { font-size: 0.8rem; color: var(--text-muted); }

    /* Animated Water Gauge */
    .tank-container { width: 100%; height: 160px; background: rgba(255,255,255,0.05); border-radius: 12px; border: 2px solid var(--border); position: relative; overflow: hidden; margin-top: 10px; }
    .tank-fill { position: absolute; bottom: 0; left: 0; width: 100%; background: linear-gradient(180deg, #48cae4 0%, #0077b6 100%); transition: height 0.6s ease; border-radius: 0 0 10px 10px; }
    .tank-label { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-size: 1.4rem; font-weight: 700; color: #fff; text-shadow: 0 2px 4px rgba(0,0,0,0.6); z-index: 2; }

    /* Control Panel */
    .controls { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; margin-top: 10px; }
    .btn { padding: 12px; border: none; border-radius: 10px; font-weight: 600; cursor: pointer; transition: all 0.2s; font-size: 0.9rem; width: 100%; }
    .btn-primary { background: #0077b6; color: #fff; }
    .btn-primary:hover { background: #0096c7; }
    .btn-danger { background: #d90429; color: #fff; }
    .btn-danger:hover { background: #ef233c; }

    footer { text-align: center; color: var(--text-muted); font-size: 0.8rem; margin-top: 25px; padding-top: 15px; border-top: 1px solid var(--border); }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="logo-group">
        <h1>💧 Smart Jal Rakshak</h1>
        <p>Intelligent Flood & Water Management for Odisha (ESP32-C6)</p>
      </div>
      <div>
        <span id="floodBadge" class="badge badge-safe">SAFE</span>
      </div>
    </header>

    <div class="grid">
      <!-- Water Level Gauge Card -->
      <div class="card">
        <h3>Water Reservoir Depth <span id="modeDisplay">AUTO</span></h3>
        <div class="metric-value"><span id="valLevel">0.0</span> <span class="metric-unit">cm</span></div>
        <div class="tank-container">
          <div id="tankFill" class="tank-fill" style="height: 0%;"></div>
          <div id="tankPct" class="tank-label">0%</div>
        </div>
        <p class="metric-sub" style="margin-top: 8px;">Capacity: 200 cm Max Depth</p>
      </div>

      <!-- Rain & Flow Rates -->
      <div class="card">
        <h3>Rain Precipitation (AO)</h3>
        <div class="metric-value"><span id="valRain">4095</span> <span class="metric-unit">ADC</span></div>
        <p id="rainStatus" class="metric-sub" style="font-size: 1rem; color: var(--accent-blue); font-weight: 600; margin-bottom: 20px;">Dry Weather</p>
        
        <h3>Diversion Flow Rate</h3>
        <div class="metric-value"><span id="valFlow">0.0</span> <span class="metric-unit">L/min</span></div>
        <p class="metric-sub">Sensor: YF-S201 Pulse Counter</p>
      </div>

      <!-- Actuators Status -->
      <div class="card">
        <h3>Hardware Actuators</h3>
        <div style="margin-bottom: 12px;">
          <p class="metric-sub">Sluice Gate Position:</p>
          <div class="metric-value" style="font-size: 1.6rem;"><span id="valGate">0</span>° <span id="gateLabel" class="metric-unit">(Closed)</span></div>
        </div>
        <div style="margin-bottom: 8px;">
          <p class="metric-sub">Pump 1 (D7) & Pump 2 (D8):</p>
          <div class="metric-value" style="font-size: 1.4rem;" id="valPumps">OFF / OFF</div>
        </div>
        <p class="metric-sub" id="alertBox" style="margin-top: 8px; color: var(--accent-yellow);">Ready</p>
      </div>
    </div>

    <!-- Manual Operator Override Panel -->
    <div class="card">
      <h3>Interactive Remote Controls</h3>
      <div class="controls">
        <button class="btn btn-primary" onclick="setMode('AUTO')">Set Mode: AUTO</button>
        <button class="btn btn-primary" onclick="setMode('MANUAL')">Set Mode: MANUAL</button>
      </div>
      <div class="controls" style="margin-top: 10px;">
        <button class="btn btn-primary" onclick="setGate(0)">Close Gate (0°)</button>
        <button class="btn btn-primary" onclick="setGate(90)">Half Gate (90°)</button>
        <button class="btn btn-primary" onclick="setGate(180)">Full Open (180°)</button>
        <button class="btn btn-danger" onclick="togglePumps()">Toggle Both Pumps</button>
      </div>
    </div>

    <footer>
      <p>Dashboard Web Server running on Seeed Studio XIAO ESP32-C6 | Firmware Version 2.0</p>
      <p>Local Access IP: <span id="espIp">--</span> | Host: http://smartjal.local</p>
    </footer>
  </div>

  <script>
    let currentPumps = false;

    async function fetchTelemetry() {
      try {
        const res = await fetch('/api/data');
        if (!res.ok) return;
        const d = await res.json();

        document.getElementById('valLevel').innerText = d.level.toFixed(1);
        document.getElementById('valRain').innerText = d.rain;
        document.getElementById('valFlow').innerText = d.flow.toFixed(1);
        document.getElementById('valGate').innerText = d.gate;
        document.getElementById('modeDisplay').innerText = d.mode;
        document.getElementById('alertBox').innerText = d.alert;
        document.getElementById('espIp').innerText = d.ip;

        let pct = Math.min(100, Math.max(0, (d.level / 200.0) * 100)).toFixed(0);
        document.getElementById('tankFill').style.height = pct + '%';
        document.getElementById('tankPct').innerText = pct + '%';

        const rEl = document.getElementById('rainStatus');
        if (d.rain < 1200) { rEl.innerText = 'Torrential Downpour (Flood Threat)'; rEl.style.color = '#ef476f'; }
        else if (d.rain < 2500) { rEl.innerText = 'Moderate Rainfall'; rEl.style.color = '#ffd166'; }
        else { rEl.innerText = 'Dry / Clear Weather'; rEl.style.color = '#48cae4'; }

        const gLabel = document.getElementById('gateLabel');
        if (d.gate >= 150) gLabel.innerText = '(Fully Open 180°)';
        else if (d.gate >= 45) gLabel.innerText = '(Partial Regulated 90°)';
        else gLabel.innerText = '(Closed 0°)';

        currentPumps = (d.p1 === 1 && d.p2 === 1);
        const pEl = document.getElementById('valPumps');
        pEl.innerText = `P1: ${d.p1 ? 'ON' : 'OFF'} | P2: ${d.p2 ? 'ON' : 'OFF'}`;
        pEl.style.color = (d.p1 || d.p2) ? '#06d6a0' : '#8d99ae';

        const b = document.getElementById('floodBadge');
        b.className = 'badge badge-' + d.state.toLowerCase();
        b.innerText = d.state;
      } catch (e) {
        console.error('Fetch error:', e);
      }
    }

    async function setMode(mode) {
      await fetch('/api/control?type=mode&val=' + mode);
      fetchTelemetry();
    }
    async function setGate(deg) {
      await fetch('/api/control?type=gate&val=' + deg);
      fetchTelemetry();
    }
    async function togglePumps() {
      const next = currentPumps ? 0 : 1;
      await fetch('/api/control?type=pumps&val=' + next);
      fetchTelemetry();
    }

    setInterval(fetchTelemetry, 1000);
    window.onload = fetchTelemetry;
  </script>
</body>
</html>
)rawliteral";

// ===========================================================================
// WEB SERVER API HANDLERS
// ===========================================================================
void handleRoot() {
  server.send(200, "text/html", DASHBOARD_HTML);
}

void handleData() {
  String json = "{";
  json += "\"level\":" + String(waterLevelCm, 1) + ",";
  json += "\"rain\":" + String(rainValue) + ",";
  json += "\"flow\":" + String(flowRate, 2) + ",";
  json += "\"gate\":" + String(sluiceServo.read()) + ",";
  json += "\"p1\":" + String(pump1State ? 1 : 0) + ",";
  json += "\"p2\":" + String(pump2State ? 1 : 0) + ",";
  json += "\"mode\":\"" + String(aiModeActive ? "AUTO" : "MANUAL") + "\",";
  json += "\"state\":\"" + floodStateStr + "\",";
  json += "\"alert\":\"" + systemAlert + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleControl() {
  if (server.hasArg("type") && server.hasArg("val")) {
    String type = server.arg("type");
    String val = server.arg("val");

    if (type == "mode") {
      aiModeActive = (val == "AUTO");
      systemAlert = "Mode set to " + val;
    } 
    else if (type == "gate") {
      aiModeActive = false; // Switch to manual
      int angle = constrain(val.toInt(), 0, 180);
      sluiceServo.write(angle);
      systemAlert = "Manual Gate: " + String(angle) + " deg";
    } 
    else if (type == "pumps") {
      aiModeActive = false; // Switch to manual
      bool state = (val.toInt() == 1);
      setPumps(state, state);
      systemAlert = state ? "Manual Both Pumps ON" : "Manual Both Pumps OFF";
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Invalid Arguments");
  }
}

// ===========================================================================
// CORE SENSOR & CONTROL TASK
// ===========================================================================
void updateSensorsAndControl() {
  float newLevel = readWaterLevel();
  if (newLevel >= 0.0) {
    waterLevelCm = newLevel;
  }
  rainValue = analogRead(RAIN_PIN);

  noInterrupts();
  int pulses = flowPulseCount;
  flowPulseCount = 0;
  interrupts();
  flowRate = (float)pulses / 15.0; // 2-second rate

  // Classify Flood Risk
  if (waterLevelCm > 60.0 || rainValue < 1000) {
    floodStateStr = "DANGER";
  } else if (waterLevelCm > 35.0 || rainValue < 2000) {
    floodStateStr = "WARN";
  } else {
    floodStateStr = "SAFE";
  }

  // Autonomous Control Logic
  if (aiModeActive) {
    if (floodStateStr == "DANGER") {
      sluiceServo.write(GATE_OPEN); // 180 degrees
      setPumps(true, true);        // Both Pump 1 (D7) and Pump 2 (D8) ON
      systemAlert = "DANGER: Full Diversion & Both Pumps Active";
    } else if (floodStateStr == "WARN") {
      sluiceServo.write(GATE_HALF); // 90 degrees
      setPumps(false, true);       // Only Pump 2 (D8) ON
      systemAlert = "WARN: Partial Sluice Gate & Pump 2 Active";
    } else {
      sluiceServo.write(GATE_CLOSED); // 0 degrees
      setPumps(false, false);      // Both Pumps OFF
      systemAlert = "SAFE: Water Level Nominal";
    }
  }

  // --- BUZZER AUDIT LOGIC ---
  unsigned long currentMillis = millis();
  if (floodStateStr == "SAFE") {
    if (previousFloodState != "SAFE") {
      playBuzzerTone(20, 1, 2); 
    }
    digitalWrite(BUZZER_PIN, LOW);
  } 
  else if (floodStateStr == "WARN") {
    if (currentMillis - lastWarnBuzzerTime >= 15000) {
      lastWarnBuzzerTime = currentMillis;
      playBuzzerTone(50, 1, 2);
    }
  } 
  else if (floodStateStr == "DANGER") {
    playBuzzerTone(10, 1, 2);
  }

  previousFloodState = floodStateStr;

  // Serial JSON Stream for monitoring
  Serial.printf("{\"level\":%.1f,\"rain\":%d,\"flow\":%.2f,\"gate\":%d,\"p1\":%d,\"p2\":%d,\"mode\":\"%s\"}\n",
                waterLevelCm, rainValue, flowRate, 
                sluiceServo.read(), pump1State ? 1 : 0, pump2State ? 1 : 0, 
                aiModeActive ? "AUTO" : "MANUAL");

  // Update I2C LCD (Rows 0, 1, 2)
  char buf[17];
  snprintf(buf, sizeof(buf), "Lvl:%4.1f %-4s", waterLevelCm, floodStateStr.c_str());
  lcd.setCursor(0, 0); lcd.print(buf);
  snprintf(buf, sizeof(buf), "R:%4d Fl:%3.1f", rainValue, flowRate);
  lcd.setCursor(0, 1); lcd.print(buf);
  snprintf(buf, sizeof(buf), "G:%3d P1:%d P2:%d", sluiceServo.read(), pump1State ? 1 : 0, pump2State ? 1 : 0);
  lcd.setCursor(0, 2); lcd.print(buf);
}

// ===========================================================================
// SETUP & MAIN LOOP
// ===========================================================================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RAIN_PIN, INPUT);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  digitalWrite(BUZZER_PIN, LOW);
  setPumps(false, false); // Initial safe state

  sluiceServo.setPeriodHertz(50);
  sluiceServo.attach(SERVO_PIN, 500, 2400);
  sluiceServo.write(GATE_CLOSED);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("SmartJalRakshak");
  lcd.setCursor(0, 1); lcd.print("Starting SJR..");

  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), countFlowPulses, RISING);

  // Connect Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  Serial.print("Connecting to Wi-Fi");
  unsigned long startWifi = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startWifi) < 10000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi Connected! IP Address: ");
    Serial.println(WiFi.localIP());

    if (MDNS.begin("smartjal")) {
      Serial.println("mDNS Responder started: http://smartjal.local");
    }

    server.on("/", handleRoot);
    server.on("/api/data", handleData);
    server.on("/api/control", handleControl);
    server.begin();
    Serial.println("HTTP Web Dashboard started on port 80!");

    Blynk.config(auth);
    Blynk.connect(1000);
  } else {
    Serial.println("Wi-Fi failed. Running offline autonomous mode.");
  }

  timer.setInterval(2000L, updateSensorsAndControl);
}

void loop() {
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
  timer.run();

  // Non-blocking smooth IP / Hostname scrolling on Row 3 (Bottom Line)
  unsigned long currentMillis = millis();
  if (currentMillis - lastScrollTime >= 350) { 
    lastScrollTime = currentMillis;

    String ipStr = (WiFi.status() == WL_CONNECTED) ? "IP:" + WiFi.localIP().toString() + "   " : "Offline:smartjal.local   ";
    int len = ipStr.length();
    
    if (len > 0) {
      String displayRow = "";
      for (int i = 0; i < 16; i++) {
        displayRow += ipStr[(scrollIndex + i) % len];
      }
      lcd.setCursor(0, 3);
      lcd.print(displayRow);
      scrollIndex = (scrollIndex + 1) % len;
    }
  }
}
