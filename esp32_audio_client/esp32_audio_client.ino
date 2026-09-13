/**
 * ============================================================
 *  ESP AI Client — Universal Library Style
 *  ESP32 · ESP32-C3 · ESP8266
 * ============================================================
 *
 *  SIRF YEH 3 CHEEZEIN BADLEIN:
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  #define WIFI_SSID      "Aapka_WiFi"                    │
 *  │  #define WIFI_PASSWORD  "Aapka_Password"                │
 *  │  #define AI_SERVER_URL  "https://xxx.onrender.com"      │
 *  └─────────────────────────────────────────────────────────┘
 *
 *  USE:
 *  ┌─────────────────────────────────────────────────────────┐
 *  │  // TEXT se poochna:                                    │
 *  │  String ans = askAI("Aaj mausam kaisa hai?");           │
 *  │  Serial.println(ans);                                   │
 *  │                                                         │
 *  │  // AUDIO se poochna (BOOT button daba ke bolo):        │
 *  │  // Button press → record → answer Serial pe            │
 *  └─────────────────────────────────────────────────────────┘
 *
 *  WIRING — INMP441 Mic:
 *  ┌────────────┬─────────┬──────────┬──────────┐
 *  │  INMP441   │ ESP32   │ ESP32-C3 │ ESP8266  │
 *  ├────────────┼─────────┼──────────┼──────────┤
 *  │  VDD       │ 3.3V    │ 3.3V     │ 3.3V     │
 *  │  GND       │ GND     │ GND      │ GND      │
 *  │  SD(DATA)  │ GPIO22  │ GPIO6    │ GPIO3*   │
 *  │  SCK       │ GPIO26  │ GPIO4    │ GPIO15   │
 *  │  WS        │ GPIO25  │ GPIO5    │ GPIO2    │
 *  │  L/R       │ GND     │ GND      │ GND      │
 *  └────────────┴─────────┴──────────┴──────────┘
 *  * ESP8266: GPIO3 = Serial RX pin (shared, debug limited)
 *
 *  LIBRARIES (Arduino Library Manager se install karo):
 *  ✅ ArduinoWebsockets  by Gil Maimon
 *  ✅ ArduinoJson        by Benoit Blanchon
 * ============================================================
 */

// ╔════════════════════════════════════════════════════════════╗
// ║          ⚙️  SIRF YEH 3 SETTINGS BADLEIN  ⚙️              ║
// ╚════════════════════════════════════════════════════════════╝

#define WIFI_SSID      "Aapka_WiFi_Naam"
#define WIFI_PASSWORD  "Aapka_WiFi_Password"

// Server URL — LOCAL ya RENDER dono kaam karte hain:
// Local:  "http://192.168.1.100:8080"    (CMD: ipconfig → IPv4)
// Render: "https://xxx.onrender.com"     (Render dashboard se)
#define AI_SERVER_URL  "https://your-server.onrender.com"

// ════════════════════════════════════════════════════════════

// ─── Ye mat badlo (auto-detect hoga) ────────────────────────
#define RECORD_BUTTON  0    // GPIO 0 = BOOT/FLASH button (har board pe hai)
#define MAX_RECORD_SEC 15   // Max recording time (seconds)

// ─── Board Detection ─────────────────────────────────────────
#if defined(ESP32)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <driver/i2s.h>
  #include <ArduinoWebsockets.h>
  #include <ArduinoJson.h>

  #if defined(CONFIG_IDF_TARGET_ESP32C3)
    #define BOARD_NAME  "ESP32-C3"
    #define I2S_SD  6
    #define I2S_SCK 4
    #define I2S_WS  5
  #else
    #define BOARD_NAME  "ESP32"
    #define I2S_SD  22
    #define I2S_SCK 26
    #define I2S_WS  25
  #endif

  #define LED_BUILTIN_PIN 2

#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <WiFiClientSecureBearSSL.h>
  #include <i2s.h>
  #include <ArduinoWebsockets.h>
  #include <ArduinoJson.h>

  #define BOARD_NAME  "ESP8266"
  #define I2S_SD  3
  #define I2S_SCK 15
  #define I2S_WS  2
  #define LED_BUILTIN_PIN LED_BUILTIN

#else
  #error "ESP32 ya ESP8266 use karo!"
#endif

using namespace websockets;

// ─── URL Parse helpers ────────────────────────────────────────
// AI_SERVER_URL se host aur port nikalna
String serverHost() {
  String url = AI_SERVER_URL;
  url.replace("https://", "");
  url.replace("http://", "");
  int slash = url.indexOf('/');
  if (slash > 0) url = url.substring(0, slash);
  int colon = url.indexOf(':');
  if (colon > 0) url = url.substring(0, colon);
  return url;
}

int serverPort() {
  String url = AI_SERVER_URL;
  if (url.startsWith("https")) return 443;
  // Local HTTP se port nikalo
  int colonAfterProto = url.indexOf("://") + 3;
  String rest = url.substring(colonAfterProto);
  int colon = rest.indexOf(':');
  if (colon < 0) return 80;
  int slash = rest.indexOf('/', colon);
  String portStr = (slash > 0) ? rest.substring(colon+1, slash) : rest.substring(colon+1);
  return portStr.toInt();
}

bool isSecure() {
  return String(AI_SERVER_URL).startsWith("https");
}

// ─── State ────────────────────────────────────────────────────
WebsocketsClient ws;
bool wsConnected  = false;
bool serverReady  = false;
bool isRecording  = false;

// Server se fetched config
int  CFG_SAMPLE_RATE  = 16000;
int  CFG_CHUNK_SIZE   = 512;
int  CFG_BITS         = 16;

// ══════════════════════════════════════════════════════════════
//   CORE FUNCTION 1: askAI(text)
//   Text bhejo → AI ka jawab lo (HTTP POST)
//   Kahan se bhi call kar sakte ho!
// ══════════════════════════════════════════════════════════════
String askAI(String question) {
  if (WiFi.status() != WL_CONNECTED) return "ERROR: WiFi nahi hai";

  String url = String(AI_SERVER_URL) + "/api/ask";
  String requestBody = "{\"text\":\"" + question + "\"}";

  Serial.println("\n📤 askAI(): " + question);

  HTTPClient http;

  #if defined(ESP8266)
    if (isSecure()) {
      BearSSL::WiFiClientSecure client;
      client.setInsecure();
      http.begin(client, url);
    } else {
      WiFiClient client;
      http.begin(client, url);
    }
  #else
    http.begin(url);
  #endif

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(15000);

  int code = http.POST(requestBody);

  if (code == 200) {
    String resp = http.getString();
    StaticJsonDocument<512> doc;
    if (!deserializeJson(doc, resp)) {
      String answer = doc["response"] | "No response";
      Serial.println("✅ AI: " + answer);
      http.end();
      return answer;
    }
  }

  Serial.println("❌ askAI error, HTTP: " + String(code));
  http.end();
  return "ERROR: Server se jawab nahi mila (HTTP " + String(code) + ")";
}

// ══════════════════════════════════════════════════════════════
//   SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println(F("\n╔══════════════════════════════════════════╗"));
  Serial.println(F("║     ESP AI Client  🤖  Universal          ║"));
  Serial.print  (F("║     Board: ")); Serial.print(BOARD_NAME);
  Serial.println(F("                         ║"));
  Serial.println(F("║     Mode : Text (HTTP) + Audio (WS)      ║"));
  Serial.println(F("╚══════════════════════════════════════════╝\n"));

  // Server URL print karo
  Serial.print(F("🌐 Server: ")); Serial.println(AI_SERVER_URL);

  pinMode(RECORD_BUTTON, INPUT_PULLUP);
  pinMode(LED_BUILTIN_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN_PIN, LOW);

  connectWiFi();
  fetchConfig();
  setupI2S();
  connectWS();

  Serial.println(F("\n✅ Ready!"));
  Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"));
  Serial.println(F("📝 TEXT  mode: askAI(\"sawaal\") call karo"));
  Serial.println(F("🎙️  AUDIO mode: BOOT button dabao aur bolo"));
  Serial.println(F("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"));

  // ── Example: startup mein ek text question karo ──
  // String ans = askAI("Mera naam kya hai?");
  // Serial.println("Answer: " + ans);
}

// ══════════════════════════════════════════════════════════════
//   LOOP
// ══════════════════════════════════════════════════════════════
void loop() {
  // WebSocket poll (audio ke liye)
  if (wsConnected) ws.poll();

  // Reconnect agar WS gir gaya
  if (!wsConnected) {
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      connectWS();
      lastRetry = millis();
    }
  }

  bool buttonHeld = (digitalRead(RECORD_BUTTON) == LOW);

  // AUDIO recording start
  if (buttonHeld && !isRecording && serverReady) {
    startAudioRecording();
  }

  // Audio stream kar raha hai
  if (isRecording) {
    streamAudioChunk();

    // Auto-stop
    static unsigned long recStart = 0;
    if (!recStart) recStart = millis();
    if (millis() - recStart > (unsigned long)MAX_RECORD_SEC * 1000) {
      Serial.println(F("\n⏰ Max time, auto-stop!"));
      stopAudioRecording();
      recStart = 0;
    }
    if (!buttonHeld) recStart = 0;
  }

  // Recording stop
  if (!buttonHeld && isRecording) {
    stopAudioRecording();
  }

  // ── YAHAN APNA CODE LIKHEIN ──────────────────────────────
  // Example: Sensor se data lo aur AI se analyze karwao
  // if (millis() % 30000 == 0) {           // har 30 sec
  //   float temp = 28.5;
  //   String q = "Temperature " + String(temp) + " degree C hai, kya theek hai?";
  //   String ans = askAI(q);
  //   Serial.println(ans);
  // }
  // ─────────────────────────────────────────────────────────
}

// ══════════════════════════════════════════════════════════════
//   WiFi Connect
// ══════════════════════════════════════════════════════════════
void connectWiFi() {
  Serial.print(F("📶 WiFi: ")); Serial.println(WIFI_SSID);
  #if defined(ESP32)
    WiFi.mode(WIFI_STA);
  #endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int t = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (++t > 40) { Serial.println(F("\n❌ WiFi failed!")); ESP.restart(); }
  }
  Serial.println(F("\n✅ WiFi OK: ") + WiFi.localIP().toString());
}

// ══════════════════════════════════════════════════════════════
//   Config Fetch (server se audio settings lo)
// ══════════════════════════════════════════════════════════════
void fetchConfig() {
  String url = String(AI_SERVER_URL) + "/config";
  Serial.print(F("⬇️  Config: ")); Serial.println(url);

  HTTPClient http;

  #if defined(ESP8266)
    if (isSecure()) {
      BearSSL::WiFiClientSecure c; c.setInsecure(); http.begin(c, url);
    } else {
      WiFiClient c; http.begin(c, url);
    }
  #else
    http.begin(url);
  #endif

  http.setTimeout(8000);
  int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<256> doc;
    if (!deserializeJson(doc, http.getString())) {
      CFG_SAMPLE_RATE = doc["sampleRate"]  | 16000;
      CFG_CHUNK_SIZE  = doc["chunkSize"]   | 512;
      CFG_BITS        = doc["bitsPerSample"]| 16;
      Serial.println(F("✅ Config loaded: SR=") + String(CFG_SAMPLE_RATE));
    }
  } else {
    Serial.println(F("⚠️  Config failed, defaults use karega"));
  }
  http.end();
}

// ══════════════════════════════════════════════════════════════
//   I2S Mic Setup
// ══════════════════════════════════════════════════════════════
void setupI2S() {
  Serial.println(F("🎤 I2S setup..."));

  #if defined(ESP32)
    i2s_config_t cfg = {
      .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate          = (uint32_t)CFG_SAMPLE_RATE,
      .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count        = 8,
      .dma_buf_len          = 64,
      .use_apll             = false,
      .tx_desc_auto_clear   = false,
      .fixed_mclk           = 0
    };
    i2s_pin_config_t pins = {
      .bck_io_num   = I2S_SCK,
      .ws_io_num    = I2S_WS,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num  = I2S_SD
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
    i2s_zero_dma_buffer(I2S_NUM_0);

  #elif defined(ESP8266)
    i2s_rxtx_begin(true, false);
    i2s_set_rate(CFG_SAMPLE_RATE);
  #endif

  Serial.println(F("✅ I2S OK"));
}

// ══════════════════════════════════════════════════════════════
//   WebSocket Connect (Audio ke liye)
// ══════════════════════════════════════════════════════════════
void connectWS() {
  Serial.println(F("🔌 WS connecting..."));

  ws.onMessage(onWsMessage);
  ws.onEvent([](WebsocketsEvent e, String d) {
    if (e == WebsocketsEvent::ConnectionOpened) {
      Serial.println(F("✅ WS connected!"));
      wsConnected = true;
      // Board info bhejo
      ws.send("CMD:INFO:{\"board\":\"" + String(BOARD_NAME) + "\",\"mode\":\"" +
              (isSecure() ? "render" : "local") + "\"}");
    }
    else if (e == WebsocketsEvent::ConnectionClosed) {
      Serial.println(F("🔌 WS disconnected"));
      wsConnected = false; serverReady = false;
    }
    else if (e == WebsocketsEvent::GotPing) { ws.pong(); }
  });

  #if defined(ESP8266)
    if (isSecure()) ws.setInsecure();
  #endif

  bool ok = isSecure()
    ? ws.connect(serverHost(), 443, "/")
    : ws.connect(serverHost(), serverPort(), "/");

  if (!ok) Serial.println(F("❌ WS connect failed!"));
}

// ══════════════════════════════════════════════════════════════
//   Audio Recording Functions
// ══════════════════════════════════════════════════════════════
void startAudioRecording() {
  Serial.println(F("\n🎙️  AUDIO MODE - Boliye!"));
  isRecording = true;
  digitalWrite(LED_BUILTIN_PIN, HIGH);
  ws.send("CMD:START");
}

void stopAudioRecording() {
  Serial.println(F("\n🛑 Recording stop, processing..."));
  isRecording = false;
  digitalWrite(LED_BUILTIN_PIN, LOW);
  ws.send("CMD:END");
}

void streamAudioChunk() {
  #if defined(ESP32)
    int32_t raw[CFG_CHUNK_SIZE / 4];
    size_t  bytesRead = 0;
    i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytesRead, portMAX_DELAY);
    if (!bytesRead) return;
    int     n    = bytesRead / 4;
    int16_t pcm[n];
    for (int i = 0; i < n; i++) pcm[i] = (int16_t)(raw[i] >> 11);
    ws.sendBinary((const char*)pcm, n * 2);

  #elif defined(ESP8266)
    int16_t pcm[CFG_CHUNK_SIZE / 4];
    int     n = 0;
    while (i2s_rx_available() && n < CFG_CHUNK_SIZE / 4) {
      pcm[n++] = (int16_t)(i2s_read_sample(false) & 0xFFFF);
    }
    if (n > 0) ws.sendBinary((const char*)pcm, n * 2);
  #endif
}

// ══════════════════════════════════════════════════════════════
//   WebSocket Message Handler
// ══════════════════════════════════════════════════════════════
void onWsMessage(WebsocketsMessage msg) {
  String m = msg.data();

  if      (m.startsWith("READY:"))    { serverReady = true; Serial.println(F("✅ Server ready! Button dabao ya askAI() call karo.")); }
  else if (m == "ACK:START")          { Serial.println(F("🔴 Recording...")); }
  else if (m.startsWith("STATUS:"))   { Serial.print(F("⏳ ")); Serial.println(m.substring(7)); }
  else if (m.startsWith("STT:"))      {
    Serial.println(F("\n┌──────────────────────────────────┐"));
    Serial.print  (F("│ 🗣️  Suna: ")); Serial.println(m.substring(4));
    Serial.println(F("└──────────────────────────────────┘"));
  }
  else if (m.startsWith("RESPONSE:")) {
    String r = m.substring(9);
    Serial.println(F("\n╔══════════════════════════════════╗"));
    Serial.println(F("║        AI KA JAWAB  🤖            ║"));
    Serial.println(F("╠══════════════════════════════════╣"));
    Serial.println(r);
    Serial.println(F("╚══════════════════════════════════╝\n"));
    serverReady = true;
  }
  else if (m.startsWith("ERROR:"))    { Serial.print(F("❌ ")); Serial.println(m.substring(6)); serverReady = true; }
  else if (m == "PONG")               { /* alive */ }
}
