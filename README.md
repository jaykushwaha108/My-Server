# 🎙️ Universal ESP Audio AI Server  v2.0

ESP32 · ESP32-C3 · ESP8266 — kisi bhi ESP se Gemini AI se real-time jawab lo!
Sirf **3 cheezein** badlo, baki sab server se auto-fetch!

## 🏗️ Architecture

```
ESP32 (INMP441 Mic)
    ↓  WebSocket (Raw 16-bit PCM)
Node.js Server
    ↓  WAV Convert
    ↓  Gemini 1.5 Flash API (Audio direct samajhta hai!)
AI Response (Text)
    ↓  WebSocket
ESP32 (Serial Monitor / Display)
```

---

## ⚡ Quick Start

### Step 1: Gemini API Key Lo (FREE)
1. Jao: https://aistudio.google.com/
2. "Get API Key" click karo
3. Key copy karo

### Step 2: Server Setup

```bash
# Project folder mein jao
cd "d:\My Server\esp32-ai-server"

# Dependencies install karo
npm install

# .env file mein API key lagao
# GEMINI_API_KEY=aapki_key_yahan

# Server chalao
npm start
```

### Step 3: Apna PC IP Address Pata Karo

```bash
# CMD mein chalao
ipconfig
# IPv4 Address dekho (jaise: 192.168.1.100)
```

### Step 4: ESP32 Setup

**Required Libraries (Arduino Library Manager mein install karo):**
- `ArduinoWebsockets` by Gil Maimon
- `ArduinoJson` by Benoit Blanchon

**esp32_audio_client.ino mein SIRF YEH 3 lines edit karo:**

```cpp
#define WIFI_SSID     "Aapka_WiFi_Naam"      // ← Apna WiFi
#define WIFI_PASSWORD "Aapka_WiFi_Password"  // ← Apna password
#define SERVER_HOST   "192.168.1.100"        // ← Apna PC IP
// Baki sab (sample rate, chunk size, etc.) server se auto-fetch! ✅
```

**Board auto-detect hoga** — same code ESP32, ESP32-C3, ESP8266 pe kaam karega!

**Upload karo ESP32 mein** aur Serial Monitor kholo (115200 baud)

---

## 🔌 INMP441 Wiring

```
INMP441 Pin  →  ESP32 Pin
──────────────────────────
VDD          →  3.3V
GND          →  GND
SD (DATA)    →  GPIO 22
SCK          →  GPIO 26
WS (LRCK)   →  GPIO 25
L/R          →  GND  (Mono ke liye)
```

## 🎮 Usage

1. Server start karo: `npm start`
2. ESP32 ko power do
3. Serial Monitor mein "Server ready!" dekho
4. **BOOT button (GPIO 0) dabao aur boliye** (LED jalega)
5. Button chodo → AI process karega
6. Jawab Serial Monitor mein aayega!

---

## 📁 Project Structure

```
esp32-ai-server/
├── server.js              ← Main WebSocket server
├── package.json           ← Dependencies
├── .env                   ← API Keys (secret!)
├── services/
│   └── geminiService.js   ← Gemini AI integration
├── utils/
│   └── wavUtils.js        ← PCM → WAV converter
└── esp32_audio_client/
    └── esp32_audio_client.ino  ← ESP32 Arduino code
```

---

## 🔧 Server Commands (ESP32 → Server)

| Command | Kaam |
|---------|------|
| `CMD:START` | Recording shuru |
| `CMD:END` | Recording band, process karo |
| `CMD:TEXT:question` | Direct text bhejo (testing) |
| `CMD:PING` | Server alive check |

## 📨 Server Responses (Server → ESP32)

| Response | Matlab |
|----------|--------|
| `READY:...` | Server ready hai |
| `ACK:START` | Recording confirm |
| `STATUS:...` | Processing status |
| `RESPONSE:text` | AI ka jawab ✅ |
| `ERROR:msg` | Kuch galat hua |

---

## ❗ Troubleshooting

| Problem | Solution |
|---------|----------|
| WiFi connect nahi ho raha | SSID/Password check karo |
| WebSocket connect nahi | Server chal raha hai? IP sahi hai? Firewall? |
| Audio nahi aa raha | INMP441 wiring check karo |
| Gemini error | API key sahi hai? Free tier limit khatam? |
| Silence/noise | I2S bit shift adjust karo (line 163 mein `>> 11` ko `>> 14` try karo) |

---

## 💰 Cost

- **Gemini API**: Free tier mein **1500 requests/day** (kaafi hai!)
- **Server**: Apna PC ya ₹0 cost
- **ESP32**: Ek baar ki hardware cost

---

## 🚀 Future Ideas

- [ ] OLED display par response show karo
- [ ] Text-to-Speech (ESP32 se speaker pe bolwao)
- [ ] Wake word detection ("Hey Gemini!")
- [ ] Multiple ESP32 clients support
