#include <WiFi.h>
#include <WebServer.h>

// ── WiFi credentials ──────────────────────────────────
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ── IR Sensor Pins (input-only ADC-capable pins) ──────
#define IR0 34   // far left
#define IR1 35
#define IR2 32   // center
#define IR3 33
#define IR4 25   // far right

// ── L298N Motor Driver Pins ───────────────────────────
#define IN1 26   // Left motor forward
#define IN2 27   // Left motor backward
#define IN3 14   // Right motor forward
#define IN4 12   // Right motor backward

// ── Speed (PWM) via LEDC ──────────────────────────────
#define PWM_FREQ   1000
#define PWM_RES    8          // 8-bit → 0–255
#define CH_IN1     0
#define CH_IN2     1
#define CH_IN3     2
#define CH_IN4     3
int motorSpeed = 180;         // default speed (0–255)

// ── State ─────────────────────────────────────────────
WebServer server(80);
String currentAction = "Stopped";
bool   autoMode      = true;
int    sensorVals[5] = {0};

// ── Motor helpers ─────────────────────────────────────
void setMotor(int chFwd, int chBwd, int spd) {
  if (spd > 0) {
    ledcWrite(chFwd, spd);
    ledcWrite(chBwd, 0);
  } else if (spd < 0) {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, -spd);
  } else {
    ledcWrite(chFwd, 0);
    ledcWrite(chBwd, 0);
  }
}

void moveForward()  { setMotor(CH_IN1,CH_IN2,motorSpeed);  setMotor(CH_IN3,CH_IN4,motorSpeed);  currentAction="Forward";   }
void turnRight()    { setMotor(CH_IN1,CH_IN2,motorSpeed);  setMotor(CH_IN3,CH_IN4,motorSpeed/3); currentAction="Turn Right"; }
void turnLeft()     { setMotor(CH_IN1,CH_IN2,motorSpeed/3); setMotor(CH_IN3,CH_IN4,motorSpeed); currentAction="Turn Left";  }
void hardRight()    { setMotor(CH_IN1,CH_IN2,motorSpeed);  setMotor(CH_IN3,CH_IN4,-motorSpeed); currentAction="Hard Right"; }
void hardLeft()     { setMotor(CH_IN1,CH_IN2,-motorSpeed); setMotor(CH_IN3,CH_IN4,motorSpeed);  currentAction="Hard Left";  }
void stopMotors()   { setMotor(CH_IN1,CH_IN2,0);           setMotor(CH_IN3,CH_IN4,0);           currentAction="Stopped";    }

// ── Line-follow logic ─────────────────────────────────
void lineFollow() {
  // LOW = on black line (IR reflected = white → HIGH)
  bool s0 = (sensorVals[0] < 2000);
  bool s1 = (sensorVals[1] < 2000);
  bool s2 = (sensorVals[2] < 2000);
  bool s3 = (sensorVals[3] < 2000);
  bool s4 = (sensorVals[4] < 2000);

  if (s2 && !s0 && !s4)       moveForward();
  else if (s1 && !s2)          turnRight();
  else if (s3 && !s2)          turnLeft();
  else if (s0)                 hardRight();
  else if (s4)                 hardLeft();
  else                         stopMotors();
}

// ── HTML dashboard ────────────────────────────────────
String buildPage() {
  String s0b = sensorVals[0]<2000?"on":"off";
  String s1b = sensorVals[1]<2000?"on":"off";
  String s2b = sensorVals[2]<2000?"on":"off";
  String s3b = sensorVals[3]<2000?"on":"off";
  String s4b = sensorVals[4]<2000?"on":"off";

  return R"rawhtml(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Line Follower</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:sans-serif;background:#0f172a;color:#e2e8f0;min-height:100vh;padding:20px}
  h1{text-align:center;font-size:1.4rem;margin-bottom:20px;color:#7dd3fc}
  .card{background:#1e293b;border-radius:12px;padding:16px;margin-bottom:16px}
  .card h2{font-size:.85rem;color:#94a3b8;margin-bottom:12px;text-transform:uppercase;letter-spacing:.05em}
  .sensors{display:flex;gap:8px;justify-content:center}
  .dot{width:44px;height:44px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:.7rem;font-weight:700;transition:.3s}
  .dot.on{background:#f59e0b;color:#1c1917}
  .dot.off{background:#334155;color:#64748b}
  .status{text-align:center;font-size:1.1rem;font-weight:600;color:#4ade80;padding:8px 0}
  .btns{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
  .btn{padding:12px;border:none;border-radius:8px;font-size:.85rem;font-weight:600;cursor:pointer;background:#334155;color:#e2e8f0;transition:.15s}
  .btn:active{transform:scale(.96)}
  .btn.active{background:#3b82f6;color:#fff}
  .btn.stop{background:#ef4444;color:#fff;grid-column:1/-1}
  label{font-size:.85rem;color:#94a3b8}
  input[type=range]{width:100%;accent-color:#3b82f6;margin-top:6px}
  .speed-row{display:flex;justify-content:space-between;margin-top:4px;font-size:.8rem;color:#4ade80}
  .mode-row{display:flex;gap:10px;margin-top:10px}
  .mode-btn{flex:1;padding:10px;border:none;border-radius:8px;font-weight:600;cursor:pointer;font-size:.82rem;transition:.15s}
  .mode-btn.active{background:#7c3aed;color:#fff}
  .mode-btn.inactive{background:#334155;color:#94a3b8}
  .ip{text-align:center;font-size:.75rem;color:#475569;margin-top:12px}
</style>
</head><body>
<h1>Line Follower Control</h1>

<div class="card">
  <h2>IR Sensors</h2>
  <div class="sensors">
    <div class="dot )rawhtml" + s0b + R"rawhtml(" id="s0">S0</div>
    <div class="dot )rawhtml" + s1b + R"rawhtml(" id="s1">S1</div>
    <div class="dot )rawhtml" + s2b + R"rawhtml(" id="s2">S2</div>
    <div class="dot )rawhtml" + s3b + R"rawhtml(" id="s3">S3</div>
    <div class="dot )rawhtml" + s4b + R"rawhtml(" id="s4">S4</div>
  </div>
</div>

<div class="card">
  <h2>Status</h2>
  <div class="status" id="action">)rawhtml" + currentAction + R"rawhtml(</div>
  <div class="mode-row">
    <button class="mode-btn )rawhtml" + String(autoMode?"active":"inactive") + R"rawhtml(" onclick="setMode(1)">Auto (Follow)</button>
    <button class="mode-btn )rawhtml" + String(!autoMode?"active":"inactive") + R"rawhtml(" onclick="setMode(0)">Manual</button>
  </div>
</div>

<div class="card">
  <h2>Manual Control</h2>
  <div class="btns">
    <div></div>
    <button class="btn" onclick="cmd('fwd')">&#9650; Forward</button>
    <div></div>
    <button class="btn" onclick="cmd('left')">&#9664; Left</button>
    <button class="btn stop" onclick="cmd('stop')">STOP</button>
    <button class="btn" onclick="cmd('right')">Right &#9654;</button>
    <div></div>
    <button class="btn" onclick="cmd('back')">&#9660; Back</button>
    <div></div>
  </div>
</div>

<div class="card">
  <h2>Motor Speed</h2>
  <label>Speed: <span id="spd">)rawhtml" + String(motorSpeed) + R"rawhtml(</span>/255</label>
  <input type="range" min="80" max="255" value=")rawhtml" + String(motorSpeed) + R"rawhtml(" oninput="setSpeed(this.value)">
  <div class="speed-row"><span>Slow</span><span>Fast</span></div>
</div>

<div class="ip">IP: )rawhtml" + WiFi.localIP().toString() + R"rawhtml(</div>

<script>
async function cmd(c){await fetch('/cmd?a='+c);}
async function setMode(m){await fetch('/mode?v='+m);}
async function setSpeed(v){document.getElementById('spd').textContent=v;await fetch('/speed?v='+v);}
async function poll(){
  const r=await fetch('/state');
  const d=await r.json();
  document.getElementById('action').textContent=d.action;
  ['s0','s1','s2','s3','s4'].forEach((id,i)=>{
    const el=document.getElementById(id);
    el.className='dot '+(d.sensors[i]?'on':'off');
  });
}
setInterval(poll,300);
</script>
</body></html>
)rawhtml";
}

// ── Web server routes ──────────────────────────────────
void handleRoot()  { server.send(200,"text/html", buildPage()); }

void handleCmd() {
  autoMode = false;
  String a = server.arg("a");
  if      (a=="fwd")   moveForward();
  else if (a=="left")  hardLeft();
  else if (a=="right") hardRight();
  else if (a=="back")  { setMotor(CH_IN1,CH_IN2,-motorSpeed); setMotor(CH_IN3,CH_IN4,-motorSpeed); currentAction="Backward"; }
  else if (a=="stop")  stopMotors();
  server.send(200,"text/plain","ok");
}

void handleMode() {
  autoMode = server.arg("v") == "1";
  if (!autoMode) stopMotors();
  server.send(200,"text/plain","ok");
}

void handleSpeed() {
  motorSpeed = server.arg("v").toInt();
  server.send(200,"text/plain","ok");
}

void handleState() {
  String j = "{\"action\":\"" + currentAction + "\",\"sensors\":[";
  for (int i=0;i<5;i++) {
    j += (sensorVals[i]<2000 ? "true" : "false");
    if (i<4) j += ",";
  }
  j += "]}";
  server.send(200,"application/json", j);
}

// ── Setup ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Motor PWM channels
  ledcSetup(CH_IN1, PWM_FREQ, PWM_RES); ledcAttachPin(IN1, CH_IN1);
  ledcSetup(CH_IN2, PWM_FREQ, PWM_RES); ledcAttachPin(IN2, CH_IN2);
  ledcSetup(CH_IN3, PWM_FREQ, PWM_RES); ledcAttachPin(IN3, CH_IN3);
  ledcSetup(CH_IN4, PWM_FREQ, PWM_RES); ledcAttachPin(IN4, CH_IN4);
  stopMotors();

  // IR pins (34,35 are input-only, no pinMode needed but safe to call)
  pinMode(IR0, INPUT); pinMode(IR1, INPUT); pinMode(IR2, INPUT);
  pinMode(IR3, INPUT); pinMode(IR4, INPUT);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  // Routes
  server.on("/",       handleRoot);
  server.on("/cmd",    handleCmd);
  server.on("/mode",   handleMode);
  server.on("/speed",  handleSpeed);
  server.on("/state",  handleState);
  server.begin();
}

// ── Loop ──────────────────────────────────────────────
void loop() {
  server.handleClient();

  // Read all 5 sensors (12-bit ADC on ESP32: 0–4095)
  sensorVals[0] = analogRead(IR0);
  sensorVals[1] = analogRead(IR1);
  sensorVals[2] = analogRead(IR2);
  sensorVals[3] = analogRead(IR3);
  sensorVals[4] = analogRead(IR4);

  if (autoMode) lineFollow();

  delay(10);
}
