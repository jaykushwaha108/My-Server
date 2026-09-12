/**
 * Gemini AI Service - Text Only
 * STT se transcribed text leke Gemini ko bhejta hai
 * Audio → STT (sttService.js) → Text → Gemini (yahan) → Response
 */

const { GoogleGenerativeAI } = require('@google/generative-ai');
require('dotenv').config();

// Gemini client initialize karo
const genAI = new GoogleGenerativeAI(process.env.GEMINI_API_KEY);

// Model - Gemini 1.5 Flash (fast + free tier available)
const model = genAI.getGenerativeModel({
  model: 'gemini-1.5-flash',
  generationConfig: {
    temperature: 0.7,
    maxOutputTokens: 500,  // Response zyada lamba na ho (ESP32 ke liye)
  }
});

/**
 * STT se transcribed text Gemini ko bhejta hai
 * @param {string} userText - STT se aaya hua transcribed question/command
 * @returns {Promise<string>} - Gemini ka jawab
 */
async function askGemini(userText) {
  if (!userText || userText.trim().length === 0) {
    throw new Error('Text empty hai, Gemini ko kuch nahi bheja ja sakta');
  }

  console.log(`🤖 Gemini ko bheja ja raha hai: "${userText}"`);

  const prompt = `Tum ek helpful AI assistant ho.
Neeche likha sawaal/request user ne voice se kaha hai (STT se convert hua):

"${userText}"

Rules:
- Agar Hindi mein poocha gaya hai to Hindi mein jawab do
- Agar English mein poocha gaya hai to English mein jawab do
- Jawab 200 words se zyada mat karo (ESP32 display ke liye)
- Seedha aur helpful jawab do
- "Aapne kaha..." ya "Maine samjha..." jaisi bakwaas mat karo`;

  const result = await model.generateContent(prompt);
  const response = result.response;

  // Check karo ki response block to nahi hua
  if (response.promptFeedback?.blockReason) {
    throw new Error(`Gemini ne block kar diya: ${response.promptFeedback.blockReason}`);
  }

  const answer = response.text().trim();
  return answer;
}

/**
 * Direct text question Gemini ko bhejta hai (CMD:TEXT testing ke liye)
 * @param {string} text - Question text
 * @returns {Promise<string>} - AI ka jawab
 */
async function processTextWithGemini(text) {
  return await askGemini(text);
}

module.exports = { askGemini, processTextWithGemini };
