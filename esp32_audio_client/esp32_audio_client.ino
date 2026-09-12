/**
 * ============================================================
 *  Universal ESP Audio AI Client  v2.0
 *  Works on: ESP32 · ESP32-C3 · ESP8266
 * ============================================================
 *
 *  SIRF YEH 3 CHEEZEIN BADLEIN:
 *    1. WIFI_SSID      → Aapka WiFi naam
 *    2. WIFI_PASSWORD  → Aapka WiFi password
 *    3. SERVER_HOST    → Server wale PC ka IP (CMD: ipconfig)
 *
 *  Baki sab config server se auto-fetch hoga! ✅
 *
 * ─── WIRING ───────────────────────────────────────────────────
 *
 *  ESP32 / ESP32-C3 → INMP441:
 *  ┌──────────────┬──────────┬──────────────┐
 *  │  INMP441     │  ESP32   │  ESP32-C3    │
 *  ├──────────────┼──────────┼──────────────┤
 *  │  VDD         │  3.3V    │  3.3V        │
 *  │  GND         │  GND     │  GND         │
 *  │  SD (DATA)   │  GPIO22  │  GPIO6       │
 *  │  SCK         │  GPIO26  │  GPIO4       │
 *  │  WS (LRCK)  │  GPIO25  │  GPIO5       │
 *  │  L/R         │  GND     │  GND (Mono)  │
 *  └──────────────┴──────────┴──────────────┘
 *
 *  ESP8266 → INMP441:
 *  ┌──────────────┬─────────────────────────────────────────┐
 *  │  INMP441     │  ESP8266 (NodeMCU)                      │
 *  ├──────────────┼─────────────────────────────────────────┤
 *  │  VDD         │  3.3V                                   │
 *  │  GND         │  GND                                    │
 *  │  SD (DATA)   │  GPIO3 (RX) ⚠️ Serial1 use karo debug   │
 *  │  SCK         │  GPIO15                                 │
 *  │  WS (LRCK)  │  GPIO2                                  │
 *  │  L/R         │  GND                                    │
 *  └──────────────┴─────────────────────────────────────────┘
 *
 *  BUTTON:
 *  ESP32/C3:  GPIO 0  (Built-in BOOT button)
 *  ESP8266:   GPIO 0  (FLASH button on NodeMCU)
 *
 *  LIBRARIES (Arduino Library Manager):
 *  ✅ ArduinoWebsockets  by Gil Maimon
 *  ✅ ArduinoJson        by Benoit Blanchon
 * ============================================================
 */

// ════════════════════════════════════════════════════════════
//   ⚙️  SIRF YEH SETTINGS BADLEIN  ⚙️
// ════════════════════════════════════════════════════════════

// ── WiFi Credentials ─────────────────────────────────────────
#define WIFI_SSID      "Aapka_WiFi_Naam"
#define WIFI_PASSWORD  "Aapka_WiFi_Password"

// ── Deployment Mode ──────────────────────────────────────────
// Option A: LOCAL server (apne PC pe)
#define SERVER_HOST    "192.168.1.100"    // ← CMD mein: ipconfig → IPv4
#define SERVER_PORT    8080
// #define USE_RENDER                     // ← LOCAL mode (default)

// Option B: RENDER cloud (comment out A, uncomment B)
// #define SERVER_HOST  "esp-ai-server.onrender.com"  // ← Render URL (without https://)
// #define SERVER_PORT  443
// #define USE_RENDER                                  // ← HTTPS + WSS enable hoga

// ════════════════════════════════════════════════════════════

// ─── Board Detection ──────────────────────────────────────────
#if defined(ESP32)
  // ESP32 (classic, WROOM, S2, S3, C3 sab)
  #include <WiFi.h>
  #include <HTTPClient.h>
  #include <driver/i2s.h>
  #include <ArduinoWebsockets.h>
  #include <ArduinoJson.h>

  #if defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-C3 pins
    #define BOARD_NAME     "ESP32-C3"
    #define I2S_SD_PIN     6
    #define I2S_SCK_PIN    4
    #define I2S_WS_PIN     5
  #elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
    // ESP32-S2 / S3 pins
    #define BOARD_NAME     "ESP32-S2/S3"
    #define I2S_SD_PIN     34
    #define I2S_SCK_PIN    26
    #define I2S_WS_PIN     25
  #else
    // ESP32 classic pins
    #define BOARD_NAME     "ESP32"
    #define I2S_SD_PIN     22
    #define I2S_SCK_PIN    26
    #define I2S_WS_PIN     25
  #endif

  #define RECORD_BUTTON  0    // BOOT button
  #define LED_PIN        2    // Built-in LED
  #define HAS_I2S_HARDWARE true

#elif defined(ESP8266)
  // ESP8266 (NodeMCU, D1 Mini, etc.)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
  #include <i2s.h>
  #include <ArduinoWebsockets.h>
  #include <ArduinoJson.h>

  #define BOARD_NAME     "ESP8266"
  // ESP8266 I2S pins are FIXED (cannot change):
  #define I2S_SD_PIN     3    // GPIO3 = RX (shared with Serial!)
  #define I2S_SCK_PIN    15   // GPIO15
  #define I2S_WS_PIN     2    // GPIO2
  #define RECORD_BUTTON  0    // FLASH button
  #define LED_PIN        2    // Built-in LED (shared with WS pin! Use LED_BUILTIN)
  #define HAS_I2S_HARDWARE true

#else
  #error "Unsupported board! ESP32, ESP32-C3 ya ESP8266 use karo."
#endif

using namespace websockets;

// ─── Runtime Config (server se fetch hoga) ───────────────────
struct DeviceConfig {
  int  sampleRate    = 16000;
  int  bitsPerSample = 16;
  int  channels      = 1;
  int  chunkSize     = 512;
  int  maxRecordSec  = 30;
  char version[10]   = "unknown";
  bool fetched       = false;
};

DeviceConfig cfg;

// ─── State ────────────────────────────────────────────────────
WebsocketsClient wsClient;
bool isConnected  = false;
bool isRecording  = false;
bool serverReady  = false;

// ─── Helpers ──────────────────────────────────────────────────
void safePrint(const String& msg)  { Serial.println(msg); }
void safePrint(const char*   msg)  { Serial.println(msg); }

// ══════════════════════════════════════════════════════════════
//   SETUP
// ══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println(F("\n╔════════════════════════════════════════╗"));
  Serial.println(F("║   Universal ESP Audio AI Client  🎙️🤖   ║"));
  Serial.print  (F("║   Board: "));
  Serial.print  (BOARD_NAME);
  Serial.println(F("                           ║"));
  Serial.println(F("╚════════════════════════════════════════╝\n"));

  pinMode(RECORD_BUTTON, INPUT_PULLUP);
  #ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
  #endif

  // WiFi → Config Fetch → I2S → WebSocket
  connectWiFi();
  fetchConfigFromServer();
  setupI2S();
  connectWebSocket();

  Serial.println(F("✅ Setup complete!"));
  Serial.println(F("📌 BOOT/FLASH button dabao aur bolein!"));
}

// ══════════════════════════════════════════════════════════════
//   MAIN LOOP
// ══════════════════════════════════════════════════════════════
void loop() {
  // WebSocket messages process karo
  if (isConnected) {
    wsClient.poll();
  } else {
    Serial.println(F("🔄 Reconnect..."));
    delay(3000);
    connectWebSocket();
    return;
  }

  bool buttonHeld = (digitalRead(RECORD_BUTTON) == LOW);

  // Start recording
  if (buttonHeld && !isRecording && serverReady) {
    startRecording();
  }

  // Stream audio
  if (isRecording) {
    sendAudioChunk();

    // Auto-stop after maxRecordSec
    static unsigned long recStart = 0;
    if (!recStart) recStart = millis();
    if (millis() - recStart > (unsigned long)cfg.maxRecordSec * 1000) {
      Serial.println(F("\n⏰ Max duration reached, auto-stop!"));
      stopRecording();
      recStart = 0;
    }
    if (!buttonHeld) recStart = 0;
  }

  // Stop recording
  if (!buttonHeld && isRecording) {
    stopRecording();
  }

  // Button daba par server ready nahi
  if (buttonHeld && !serverReady && isConnected) {
    Serial.println(F("⏳ Server ready nahi hai..."));
    delay(300);
  }
}

// ══════════════════════════════════════════════════════════════
//   WIFI
// ══════════════════════════════════════════════════════════════
void connectWiFi() {
  Serial.print(F("📶 WiFi: "));
  Serial.println(WIFI_SSID);

  #if defined(ESP32)
    WiFi.mode(WIFI_STA);
  #endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
    if (++tries > 40) {
      Serial.println(F("\n❌ WiFi failed! Restarting..."));
      ESP.restart();
    }
  }

  Serial.println(F("\n✅ WiFi Connected!"));
  Serial.print(F("   IP: "));
  Serial.println(WiFi.localIP());
}

// ══════════════════════════════════════════════════════════════
//   CONFIG FETCH FROM SERVER  (http://SERVER_IP:PORT/config)
// ══════════════════════════════════════════════════════════════
void fetchConfigFromServer() {
  // USE_RENDER = https://, else http://
  #ifdef USE_RENDER
    String url = "https://";
  #else
    String url = "http://";
  #endif

  url += SERVER_HOST;

  #ifndef USE_RENDER
    // Port sirf local mein chahiye; Render pe 443 default hota hai
    url += ":";
    url += SERVER_PORT;
  #endif

  url += "/config";

  Serial.print(F("⬇️  Config fetch: "));
  Serial.println(url);

  HTTPClient http;

  #if defined(ESP8266)
    #ifdef USE_RENDER
      // ESP8266 + HTTPS: needs WiFiClientSecure
      WiFiClientSecure secClient;
      secClient.setInsecure();           // SSL verify skip (simple setup)
      http.begin(secClient, url);
    #else
      WiFiClient wifiClient;
      http.begin(wifiClient, url);
    #endif
  #else
    // ESP32 handles HTTPS automatically
    http.begin(url);
  #endif

  http.setTimeout(5000);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println(F("✅ Config received:"));
    Serial.println(payload);

    // JSON parse karo
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      cfg.sampleRate    = doc["sampleRate"]    | 16000;
      cfg.bitsPerSample = doc["bitsPerSample"] | 16;
      cfg.channels      = doc["channels"]      | 1;
      cfg.chunkSize     = doc["chunkSize"]     | 512;
      cfg.maxRecordSec  = doc["maxRecordSec"]  | 30;
      strlcpy(cfg.version, doc["version"] | "?", sizeof(cfg.version));
      cfg.fetched = true;

      Serial.println(F("📋 Loaded config:"));
      Serial.print  (F("   SampleRate    : ")); Serial.println(cfg.sampleRate);
      Serial.print  (F("   BitsPerSample : ")); Serial.println(cfg.bitsPerSample);
      Serial.print  (F("   Channels      : ")); Serial.println(cfg.channels);
      Serial.print  (F("   ChunkSize     : ")); Serial.println(cfg.chunkSize);
      Serial.print  (F("   MaxRecordSec  : ")); Serial.println(cfg.maxRecordSec);
      Serial.print  (F("   ServerVersion : ")); Serial.println(cfg.version);
    } else {
      Serial.print(F("⚠️  JSON parse error: ")); Serial.println(err.c_str());
      Serial.println(F("   Default config use ho rahi hai."));
    }
  } else {
    Serial.print(F("⚠️  Config fetch failed (HTTP ")); Serial.print(httpCode); Serial.println(F("). Default use karega."));
  }

  http.end();
}

// ══════════════════════════════════════════════════════════════
//   I2S MIC SETUP
// ══════════════════════════════════════════════════════════════
void setupI2S() {
  Serial.println(F("🎤 I2S Mic setup..."));

  #if defined(ESP32)
    i2s_config_t i2sCfg = {
      .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate          = (uint32_t)cfg.sampleRate,
      .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 → 32bit, we shift to 16
      .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count        = 8,
      .dma_buf_len          = 64,
      .use_apll             = false,
      .tx_desc_auto_clear   = false,
      .fixed_mclk           = 0
    };

    i2s_pin_config_t pinCfg = {
      .bck_io_num   = I2S_SCK_PIN,
      .ws_io_num    = I2S_WS_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num  = I2S_SD_PIN
    };

    if (i2s_driver_install(I2S_NUM_0, &i2sCfg, 0, NULL) != ESP_OK) {
      Serial.println(F("❌ I2S driver install failed!"));
      return;
    }
    if (i2s_set_pin(I2S_NUM_0, &pinCfg) != ESP_OK) {
      Serial.println(F("❌ I2S pins set failed!"));
      return;
    }
    i2s_zero_dma_buffer(I2S_NUM_0);

  #elif defined(ESP8266)
    // ESP8266 I2S (Fixed pins: DATA=GPIO3, CLK=GPIO15, WS=GPIO2)
    i2s_rxtx_begin(true, false);           // RX=true, TX=false
    i2s_set_rate(cfg.sampleRate);
    Serial.println(F("⚠️  ESP8266: I2S pins fixed (DATA=GPIO3, CLK=GPIO15, WS=GPIO2)"));
    Serial.println(F("⚠️  GPIO3 = Serial RX shared! Debug limited on 8266."));
  #endif

  Serial.println(F("✅ I2S Mic ready!"));
}

// ══════════════════════════════════════════════════════════════
//   WEBSOCKET CONNECT  (ws:// local | wss:// Render)
// ══════════════════════════════════════════════════════════════
void connectWebSocket() {
  #ifdef USE_RENDER
    // Render = wss:// (secure, port 443)
    String url = "wss://";
    url += SERVER_HOST;
    url += "/";
    Serial.print(F("🔌 WSS (Render): ")); Serial.println(url);
  #else
    // Local = ws:// 
    String url = "ws://";
    url += SERVER_HOST;
    url += ":";
    url += SERVER_PORT;
    url += "/";
    Serial.print(F("🔌 WS (Local): ")); Serial.println(url);
  #endif

  wsClient.onMessage(onWsMessage);
  wsClient.onEvent([](WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
      Serial.println(F("✅ WebSocket Connected!"));
      isConnected = true;

      // Device info server ko bhejo
      String info = "CMD:INFO:{\"board\":\"";
      info += BOARD_NAME;
      info += "\",\"sampleRate\":";
      info += cfg.sampleRate;
      info += ",\"configFetched\":";
      info += cfg.fetched ? "true" : "false";
      #ifdef USE_RENDER
        info += ",\"mode\":\"render\"";
      #else
        info += ",\"mode\":\"local\"";
      #endif
      info += "}";
      wsClient.send(info);
    }
    else if (event == WebsocketsEvent::ConnectionClosed) {
      Serial.println(F("🔌 WebSocket Disconnected!"));
      isConnected = false;
      serverReady = false;
    }
    else if (event == WebsocketsEvent::GotPing) {
      wsClient.pong();
    }
  });

  #ifdef USE_RENDER
    // ESP32 + WSS: ArduinoWebsockets handles SSL automatically
    // ESP8266 + WSS: Set insecure (no cert verify)
    #if defined(ESP8266)
      wsClient.setInsecure();
    #endif
    bool ok = wsClient.connect(SERVER_HOST, 443, "/");
  #else
    bool ok = wsClient.connect(SERVER_HOST, SERVER_PORT, "/");
  #endif

  if (!ok) {
    #ifdef USE_RENDER
      Serial.println(F("❌ WS connect failed! Render URL sahi hai? Internet on hai?"));
    #else
      Serial.println(F("❌ WS connect failed! Server chal raha hai? IP sahi hai?"));
    #endif
    isConnected = false;
  }
}

// ══════════════════════════════════════════════════════════════
//   RECORDING CONTROL
// ══════════════════════════════════════════════════════════════
void startRecording() {
  Serial.println(F("\n🎙️ Recording SHURU - BOLIYE!"));
  isRecording = true;
  #ifdef LED_PIN
    digitalWrite(LED_PIN, HIGH);
  #endif
  wsClient.send("CMD:START");
}

void stopRecording() {
  Serial.println(F("\n🛑 Recording BAND. Processing..."));
  isRecording = false;
  #ifdef LED_PIN
    digitalWrite(LED_PIN, LOW);
  #endif
  wsClient.send("CMD:END");
  Serial.println(F("⏳ AI jawab aa raha hai..."));
}

// ══════════════════════════════════════════════════════════════
//   AUDIO CHUNK SEND
// ══════════════════════════════════════════════════════════════
void sendAudioChunk() {
  #if defined(ESP32)
    int32_t raw32[cfg.chunkSize / 4];
    size_t  bytesRead = 0;

    i2s_read(I2S_NUM_0, raw32, sizeof(raw32), &bytesRead, portMAX_DELAY);
    if (bytesRead == 0) return;

    int       samples = bytesRead / 4;
    int16_t   pcm16[samples];

    for (int i = 0; i < samples; i++) {
      pcm16[i] = (int16_t)(raw32[i] >> 11);  // 32-bit → 16-bit (INMP441)
    }

    wsClient.sendBinary((const char*)pcm16, samples * 2);

  #elif defined(ESP8266)
    // ESP8266 I2S returns 32-bit values (upper 16 = left, lower 16 = right)
    uint32_t raw32[cfg.chunkSize / 4];
    int      count = 0;
    int16_t  pcm16[cfg.chunkSize / 4];

    while (i2s_rx_available() && count < (cfg.chunkSize / 4)) {
      uint32_t sample = i2s_read_sample(false);
      pcm16[count++]  = (int16_t)(sample & 0xFFFF);
    }

    if (count > 0) {
      wsClient.sendBinary((const char*)pcm16, count * 2);
    }
  #endif
}

// ══════════════════════════════════════════════════════════════
//   WEBSOCKET MESSAGE HANDLER
// ══════════════════════════════════════════════════════════════
void onWsMessage(WebsocketsMessage msg) {
  String m = msg.data();

  if (m.startsWith("READY:")) {
    serverReady = true;
    Serial.println(F("\n✅ Server ready!"));
    Serial.print  (F("   ")); Serial.println(m.substring(6));
    Serial.println(F("📌 BOOT button dabao aur bolein!"));
  }
  else if (m == "ACK:START") {
    Serial.println(F("✅ Recording server ne confirm ki."));
  }
  else if (m.startsWith("STATUS:")) {
    Serial.print(F("⏳ ")); Serial.println(m.substring(7));
  }
  else if (m.startsWith("STT:")) {
    String heard = m.substring(4);
    Serial.println(F("\n┌────────────────────────────────────┐"));
    Serial.print  (F("│ 🗣️  Suna: "));
    Serial.println(heard);
    Serial.println(F("└────────────────────────────────────┘"));
  }
  else if (m.startsWith("RESPONSE:")) {
    String resp = m.substring(9);
    Serial.println(F("\n╔════════════════════════════════════╗"));
    Serial.println(F("║         AI KA JAWAB  🤖             ║"));
    Serial.println(F("╠════════════════════════════════════╣"));
    Serial.println(resp);
    Serial.println(F("╚════════════════════════════════════╝\n"));
    Serial.println(F("📌 Button dabao aur dobara puchho!"));
    serverReady = true;
  }
  else if (m == "PONG") {
    Serial.println(F("🏓 Pong!"));
  }
  else if (m.startsWith("ERROR:")) {
    Serial.print(F("❌ Error: ")); Serial.println(m.substring(6));
    serverReady = true;
  }
  else {
    Serial.print(F("📨 ")); Serial.println(m);
  }
}
