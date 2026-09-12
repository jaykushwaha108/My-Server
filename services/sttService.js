/**
 * STT Service - Speech To Text
 * Google Cloud Speech-to-Text REST API use karta hai
 * Raw PCM audio → Transcribed Text
 *
 * Setup:
 *  1. Google Cloud Console jao: https://console.cloud.google.com/
 *  2. "Speech-to-Text API" enable karo
 *  3. "APIs & Services" → "Credentials" → "Create API Key"
 *  4. Key .env mein GOOGLE_STT_API_KEY=... mein daalo
 *
 * Free Tier: 60 minutes/month FREE!
 * Hindi + English dono support karta hai
 */

const https = require('https');

const SAMPLE_RATE   = parseInt(process.env.SAMPLE_RATE)       || 16000;
const CHANNELS      = parseInt(process.env.CHANNELS)           || 1;
const STT_API_KEY   = process.env.GOOGLE_STT_API_KEY;

/**
 * Raw PCM buffer se text transcribe karta hai
 * @param {Buffer} pcmBuffer - Raw 16-bit PCM audio data (ESP32 se)
 * @returns {Promise<string>} - Transcribed text
 */
async function transcribeAudio(pcmBuffer) {
  if (!STT_API_KEY || STT_API_KEY === 'your_google_stt_api_key_here') {
    throw new Error('GOOGLE_STT_API_KEY .env mein set nahi hai!');
  }

  // Raw PCM ko base64 mein convert karo
  const audioBase64 = pcmBuffer.toString('base64');

  // Google STT request body
  const requestBody = JSON.stringify({
    config: {
      encoding: 'LINEAR16',             // 16-bit PCM
      sampleRateHertz: SAMPLE_RATE,     // 16000 Hz
      audioChannelCount: CHANNELS,      // 1 = Mono
      languageCode: 'hi-IN',            // Primary: Hindi
      alternativeLanguageCodes: [       // Fallback languages
        'en-IN',                        // English (Indian)
        'en-US'                         // English (US)
      ],
      enableAutomaticPunctuation: true, // Punctuation add karo
      model: 'latest_short',            // Short audio ke liye optimized
      useEnhanced: true                 // Better accuracy (free tier mein bhi available)
    },
    audio: {
      content: audioBase64              // Base64 PCM audio
    }
  });

  console.log('🗣️  Google STT se transcribe ho raha hai...');
  const response = await makeGoogleSTTRequest(requestBody);

  // Response parse karo
  if (!response.results || response.results.length === 0) {
    throw new Error('Audio mein koi baat samajh nahi aayi. Thoda zyada aawaaz se boliye!');
  }

  // Saare results combine karo
  const transcript = response.results
    .map(result => result.alternatives[0].transcript)
    .join(' ')
    .trim();

  // Confidence score bhi log karo
  const confidence = response.results[0]?.alternatives[0]?.confidence;
  if (confidence) {
    console.log(`✅ STT Result: "${transcript}"`);
    console.log(`   Confidence: ${(confidence * 100).toFixed(1)}%`);
  } else {
    console.log(`✅ STT Result: "${transcript}"`);
  }

  return transcript;
}

/**
 * Google Cloud STT REST API ko HTTPS request bhejta hai
 * @param {string} body - JSON request body
 * @returns {Promise<object>} - Parsed JSON response
 */
function makeGoogleSTTRequest(body) {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: 'speech.googleapis.com',
      path: `/v1/speech:recognize?key=${STT_API_KEY}`,
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(body)
      }
    };

    const req = https.request(options, (res) => {
      let data = '';

      res.on('data', (chunk) => { data += chunk; });

      res.on('end', () => {
        try {
          const parsed = JSON.parse(data);

          // Error check karo
          if (parsed.error) {
            const msg = parsed.error.message || 'Google STT API error';
            const code = parsed.error.code;

            if (code === 403) {
              reject(new Error(`STT API key invalid hai ya Speech API enable nahi ki: ${msg}`));
            } else if (code === 400) {
              reject(new Error(`Audio format galat hai: ${msg}`));
            } else {
              reject(new Error(`STT Error (${code}): ${msg}`));
            }
            return;
          }

          resolve(parsed);
        } catch (e) {
          reject(new Error(`STT response parse nahi hua: ${e.message}`));
        }
      });
    });

    req.on('error', (err) => {
      reject(new Error(`STT network error: ${err.message}`));
    });

    req.setTimeout(15000, () => {
      req.destroy();
      reject(new Error('STT request timeout (15s)'));
    });

    req.write(body);
    req.end();
  });
}

module.exports = { transcribeAudio };
