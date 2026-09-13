/**
 * ============================================================
 *   ESP AI Audio Server  v2.0
 *   Flow: ESP Audio → STT (Google) → Gemini AI → Response → ESP
 * ============================================================
 *
 *  HTTP Endpoints:
 *    GET  /config   → ESP device config fetch karta hai (JSON)
 *    GET  /health   → Server health check
 *
 *  WebSocket Commands (ESP → Server):
 *    CMD:START        → Recording shuru
 *    CMD:END          → Recording khatam, process karo
 *    CMD:TEXT:sawaal  → Direct text question (testing)
 *    CMD:PING         → Alive check
 *
 *  WebSocket Responses (Server → ESP):
 *    READY:msg        → Server ready
 *    ACK:START        → Recording confirm
 *    STATUS:msg       → Processing status
 *    STT:text         → Transcribed text (STT result)
 *    RESPONSE:text    → Gemini AI ka final jawab
 *    ERROR:msg        → Error
 */

require('dotenv').config();
const http      = require('http');
const WebSocket = require('ws');
const { getAudioDuration } = require('./utils/wavUtils');
const { transcribeAudio }  = require('./services/sttService');
const { askGemini, processTextWithGemini } = require('./services/geminiService');

// ─── Config ──────────────────────────────────────────────────
const PORT            = parseInt(process.env.PORT)            || 8080;
const SAMPLE_RATE     = parseInt(process.env.SAMPLE_RATE)     || 16000;
const BITS_PER_SAMPLE = parseInt(process.env.BITS_PER_SAMPLE) || 16;
const CHANNELS        = parseInt(process.env.CHANNELS)        || 1;
const SERVER_VERSION  = '2.0.0';

// ─── Startup Checks ──────────────────────────────────────────
if (!process.env.GEMINI_API_KEY || process.env.GEMINI_API_KEY === 'your_gemini_api_key_here') {
  console.error('❌ ERROR: GEMINI_API_KEY .env mein set nahi hai!');
  console.error('   Key lene ke liye: https://aistudio.google.com/');
  process.exit(1);
}

if (!process.env.GOOGLE_STT_API_KEY || process.env.GOOGLE_STT_API_KEY === 'your_google_stt_api_key_here') {
  console.error('❌ ERROR: GOOGLE_STT_API_KEY .env mein set nahi hai!');
  console.error('   Key lene ke liye: https://console.cloud.google.com/');
  process.exit(1);
}

// ─── Config object (ESP ko bheja jaata hai) ──────────────────
const DEVICE_CONFIG = {
  version:       SERVER_VERSION,
  sampleRate:    SAMPLE_RATE,
  bitsPerSample: BITS_PER_SAMPLE,
  channels:      CHANNELS,
  wsPath:        '/',
  maxRecordSec:  30,      // Max recording duration
  chunkSize:     512,     // Bytes per audio chunk
  ready:         true
};

// ─── HTTP Server (Config endpoint + WS upgrade) ───────────────
const httpServer = http.createServer((req, res) => {

  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type, X-API-Token');

  // OPTIONS preflight
  if (req.method === 'OPTIONS') {
    res.writeHead(204);
    res.end();
    return;
  }

  // GET /config → ESP device config fetch karta hai
  if (req.url === '/config' && req.method === 'GET') {
    const configJson = JSON.stringify(DEVICE_CONFIG, null, 2);
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(configJson);
    console.log(`📋 [${timestamp()}] Config request aayi: ${req.socket.remoteAddress}`);
    return;
  }

  // GET /health → Health check
  if (req.url === '/health' && req.method === 'GET') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      status: 'ok',
      version: SERVER_VERSION,
      uptime: Math.floor(process.uptime()),
      clients: wss ? wss.clients.size : 0
    }));
    return;
  }

  // ─── POST /api/ask → TEXT question → AI answer ────────────────
  // ESP directly text bhejta hai, JSON response milta hai
  // Body: {"text": "aapka sawaal"}
  // Response: {"ok": true, "response": "AI ka jawab"}
  if (req.url === '/api/ask' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => { body += chunk; });
    req.on('end', async () => {
      const clientIP = req.socket.remoteAddress;
      try {
        const parsed   = JSON.parse(body);
        const question = (parsed.text || '').trim();

        if (!question) {
          res.writeHead(400, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ ok: false, error: 'text field empty hai' }));
          return;
        }

        console.log(`\n📝 [${timestamp()}] /api/ask from ${clientIP}`);
        console.log(`   Question: "${question}"`);

        const aiResponse = await askGemini(question);
        console.log(`   Answer  : "${aiResponse}"\n`);

        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ ok: true, response: aiResponse }));

      } catch (err) {
        console.error(`❌ /api/ask error: ${err.message}`);
        res.writeHead(500, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ ok: false, error: err.message }));
      }
    });
    return;
  }

  // Default 404
  res.writeHead(404, { 'Content-Type': 'text/plain' });
  res.end('ESP AI Server v2\n  GET  /config\n  GET  /health\n  POST /api/ask\n  WS   /  (audio streaming)\n');
});

// ─── WebSocket Server (same port as HTTP) ────────────────────
const wss = new WebSocket.Server({ server: httpServer });

// ─── Banner ───────────────────────────────────────────────────
httpServer.listen(PORT, () => {
  console.log('');
  console.log('╔═══════════════════════════════════════════════════╗');
  console.log('║     ESP Audio AI Server  🎙️ → 📝 → 🤖  v' + SERVER_VERSION + '    ║');
  console.log('╠═══════════════════════════════════════════════════╣');
  console.log(`║  HTTP+WS Port : ${PORT}                                ║`);
  console.log(`║  Config URL   : http://YOUR_IP:${PORT}/config          ║`);
  console.log(`║  Pipeline     : Audio → STT → Gemini → Response   ║`);
  console.log(`║  STT          : Google Cloud Speech-to-Text        ║`);
  console.log(`║  AI Model     : Gemini 1.5 Flash                   ║`);
  console.log(`║  Audio Format : ${SAMPLE_RATE}Hz · ${BITS_PER_SAMPLE}bit · ${CHANNELS}ch Mono           ║`);
  console.log('╚═══════════════════════════════════════════════════╝');
  console.log('');
  console.log('💡 ESP device ke .ino mein sirf WiFi + Server IP daalo');
  console.log('   Baki sab config /config se auto-fetch hoga!\n');
  console.log('⏳ ESP device connection ka wait kar raha hun...\n');
});

// ─── WebSocket Connection Handler ─────────────────────────────
wss.on('connection', (ws, req) => {
  const clientIP = req.socket.remoteAddress;
  const userAgent = req.headers['user-agent'] || 'Unknown Device';
  console.log(`✅ [${timestamp()}] Device Connected!`);
  console.log(`   IP: ${clientIP}`);
  console.log(`   Device: ${userAgent}`);

  // Per-connection state
  let audioChunks    = [];
  let isRecording    = false;
  let recordingStart = null;
  let deviceInfo     = {};

  // ─── Message Handler ───────────────────────────────────────
  ws.on('message', async (data, isBinary) => {

    // ── Binary = audio chunk ──────────────────────────────
    if (isBinary) {
      if (!isRecording) {
        console.warn('⚠️  Audio aaya but CMD:START nahi mila! Ignore...');
        return;
      }
      audioChunks.push(Buffer.from(data));

      // Progress every 20 chunks
      if (audioChunks.length % 20 === 0) {
        const totalBytes = audioChunks.reduce((s, c) => s + c.length, 0);
        const dur = getAudioDuration(totalBytes, deviceInfo.sampleRate || SAMPLE_RATE, CHANNELS, BITS_PER_SAMPLE);
        process.stdout.write(`\r📦 Chunks: ${audioChunks.length} | ${(totalBytes/1024).toFixed(1)}KB | ${dur}s   `);
      }
      return;
    }

    // ── Text = command ────────────────────────────────────
    const message = data.toString().trim();
    console.log(`\n📨 [${timestamp()}] "${message}"`);

    // ── CMD:INFO:{json} → Device info (optional) ─────────
    if (message.startsWith('CMD:INFO:')) {
      try {
        deviceInfo = JSON.parse(message.substring(9));
        console.log(`📱 Device info: Board=${deviceInfo.board}, SR=${deviceInfo.sampleRate}Hz`);
      } catch (e) { /* ignore */ }
      return;
    }

    // ── CMD:START → Recording shuru ───────────────────────
    if (message === 'CMD:START') {
      audioChunks    = [];
      isRecording    = true;
      recordingStart = Date.now();
      console.log('🎙️  Recording shuru!');
      safeSend(ws, 'ACK:START');
      return;
    }

    // ── CMD:END → Process ────────────────────────────────
    if (message === 'CMD:END') {
      isRecording = false;

      if (audioChunks.length === 0) {
        safeSend(ws, 'ERROR:Koi audio nahi mila. Button hold karke bolein.');
        return;
      }

      const pcmBuffer  = Buffer.concat(audioChunks);
      const sr         = deviceInfo.sampleRate || SAMPLE_RATE;
      const duration   = getAudioDuration(pcmBuffer.length, sr, CHANNELS, BITS_PER_SAMPLE);
      const recSec     = ((Date.now() - recordingStart) / 1000).toFixed(1);

      console.log(`\n📊 Audio:`);
      console.log(`   Size: ${(pcmBuffer.length/1024).toFixed(1)}KB | Duration: ${duration}s | RecTime: ${recSec}s`);

      audioChunks = [];

      try {
        // STEP 1: STT → Audio to Text
        safeSend(ws, 'STATUS:Aawaz text mein badli ja rahi hai...');
        console.log('\n🗣️  STEP 1: Google STT...');

        const transcribedText = await transcribeAudio(pcmBuffer);
        console.log(`📝 STT: "${transcribedText}"`);
        safeSend(ws, `STT:${transcribedText}`);

        // STEP 2: Gemini → Text to Answer
        safeSend(ws, 'STATUS:AI jawab dhoondh raha hai...');
        console.log('\n🤖 STEP 2: Gemini AI...');

        const aiResponse = await askGemini(transcribedText);
        console.log(`💬 Gemini: "${aiResponse}"\n`);

        // STEP 3: Response wapas bhejo
        safeSend(ws, `RESPONSE:${aiResponse}`);
        console.log(`✅ [${timestamp()}] Done!\n`);

      } catch (err) {
        console.error(`❌ Error: ${err.message}`);
        safeSend(ws, `ERROR:${err.message}`);
      }

      return;
    }

    // ── CMD:TEXT:q → Direct text (testing) ───────────────
    if (message.startsWith('CMD:TEXT:')) {
      const q = message.substring(9).trim();
      safeSend(ws, 'STATUS:Processing...');
      try {
        const r = await processTextWithGemini(q);
        safeSend(ws, `RESPONSE:${r}`);
      } catch (e) {
        safeSend(ws, `ERROR:${e.message}`);
      }
      return;
    }

    // ── CMD:PING ──────────────────────────────────────────
    if (message === 'CMD:PING') {
      safeSend(ws, 'PONG');
      return;
    }

    console.warn(`⚠️  Unknown: "${message}"`);
  });

  ws.on('close', (code) => {
    console.log(`\n🔌 [${timestamp()}] Device disconnected (${code})`);
    audioChunks = [];
    isRecording = false;
  });

  ws.on('error', (err) => {
    console.error(`❌ WS Error: ${err.message}`);
  });

  // Welcome + send config URL hint
  safeSend(ws, `READY:Server v${SERVER_VERSION} ready! Bolo, main sun raha hun.`);
});

// ─── Server Events ────────────────────────────────────────────
httpServer.on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.error(`❌ Port ${PORT} already use mein hai! .env mein PORT badlein.`);
    process.exit(1);
  }
  console.error(`❌ Server Error: ${err.message}`);
});

// ─── Helpers ─────────────────────────────────────────────────
function safeSend(ws, msg) {
  if (ws.readyState === WebSocket.OPEN) ws.send(msg);
}

function timestamp() {
  return new Date().toLocaleTimeString('en-IN');
}

// ─── Graceful Shutdown ────────────────────────────────────────
process.on('SIGINT', () => {
  console.log('\n🛑 Server band ho raha hai...');
  httpServer.close(() => {
    console.log('✅ Server closed.');
    process.exit(0);
  });
});
