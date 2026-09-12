/**
 * WAV Utility - PCM raw audio ko WAV format mein convert karta hai
 * ESP32 se aata hai raw 16-bit PCM → WAV header lagata hai
 */

/**
 * Raw PCM Buffer se WAV Buffer banata hai
 * @param {Buffer} pcmBuffer - Raw PCM audio data
 * @param {number} sampleRate - Sample rate (default: 16000)
 * @param {number} channels - Channels count (default: 1 = mono)
 * @param {number} bitsPerSample - Bit depth (default: 16)
 * @returns {Buffer} - Complete WAV file buffer
 */
function pcmToWav(pcmBuffer, sampleRate = 16000, channels = 1, bitsPerSample = 16) {
  const dataLength = pcmBuffer.length;
  const wavBuffer = Buffer.alloc(44 + dataLength);

  // --- RIFF Chunk ---
  wavBuffer.write('RIFF', 0, 'ascii');                          // ChunkID
  wavBuffer.writeUInt32LE(36 + dataLength, 4);                  // ChunkSize
  wavBuffer.write('WAVE', 8, 'ascii');                          // Format

  // --- fmt Sub-chunk ---
  wavBuffer.write('fmt ', 12, 'ascii');                         // Subchunk1ID
  wavBuffer.writeUInt32LE(16, 16);                              // Subchunk1Size (PCM = 16)
  wavBuffer.writeUInt16LE(1, 20);                               // AudioFormat (1 = PCM)
  wavBuffer.writeUInt16LE(channels, 22);                        // NumChannels
  wavBuffer.writeUInt32LE(sampleRate, 24);                      // SampleRate
  wavBuffer.writeUInt32LE(sampleRate * channels * bitsPerSample / 8, 28); // ByteRate
  wavBuffer.writeUInt16LE(channels * bitsPerSample / 8, 32);   // BlockAlign
  wavBuffer.writeUInt16LE(bitsPerSample, 34);                   // BitsPerSample

  // --- data Sub-chunk ---
  wavBuffer.write('data', 36, 'ascii');                         // Subchunk2ID
  wavBuffer.writeUInt32LE(dataLength, 40);                      // Subchunk2Size
  pcmBuffer.copy(wavBuffer, 44);                                // PCM data copy karo

  return wavBuffer;
}

/**
 * Audio duration calculate karta hai
 * @param {number} byteLength - PCM data bytes
 * @param {number} sampleRate - Sample rate
 * @param {number} channels - Channels
 * @param {number} bitsPerSample - Bit depth
 * @returns {string} - Duration in seconds (formatted)
 */
function getAudioDuration(byteLength, sampleRate = 16000, channels = 1, bitsPerSample = 16) {
  const seconds = byteLength / (sampleRate * channels * (bitsPerSample / 8));
  return seconds.toFixed(2);
}

module.exports = { pcmToWav, getAudioDuration };
