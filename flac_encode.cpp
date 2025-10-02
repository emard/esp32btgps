// cd ~/Arduino/libraries
// git clone https://github.com/pschatzmann/arduino-libflac
// git clone https://github.com/pschatzmann/codec-ogg
// alternative method:
// zip -r arduino-libflac.zip arduino-libflac
// zip -r codec-ogg.zip codec-ogg
// Sketch -> Include Library -> Add .ZIP Library... -> arduino-libflac.zip and codec-ogg.zip

#include <SD_MMC.h>
#include "flac.h"
#include "flac_encode.h"

FLAC__StreamEncoder *encoder;

File cb_outputFile;

// Custom write callback for FLAC encoder
FLAC__StreamEncoderWriteStatus write_callback(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data) {
    if (cb_outputFile.write(buffer, bytes) != bytes) {
        Serial.println("Error writing to FLAC file!");
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

// 0:fail
// 1:success
// expected encoding speed is about 600 K/s
int flac_encode(File &outputFile, File &inputFile)
{
    // Read WAV header to confirm format
    wav_header_t wavHeader;
    if (inputFile.read((uint8_t*)&wavHeader, sizeof(wavHeader)) != sizeof(wavHeader)) {
        Serial.println("Failed to read WAV header.");
        inputFile.close();
        return 0;
    }

    // Verify WAV header against expected values
    if (wavHeader.numChannels != EXPECTED_CHANNELS ||
        wavHeader.sampleRate != EXPECTED_SAMPLE_RATE ||
        wavHeader.bitsPerSample != EXPECTED_BITS_PER_CHANNEL) {
        Serial.println("WAV file format mismatch!");
        Serial.printf("Expected: %d channels, %d Hz, %d bps\n", EXPECTED_CHANNELS, EXPECTED_SAMPLE_RATE, EXPECTED_BITS_PER_CHANNEL);
        Serial.printf("Got: %d channels, %d Hz, %d bps\n", wavHeader.numChannels, wavHeader.sampleRate, wavHeader.bitsPerSample);
        inputFile.close();
        return 0;
    }
    // Serial.println("WAV header verified.");
    cb_outputFile = outputFile; // copy file pointer to callback

    // Create a new FLAC encoder instance
    encoder = FLAC__stream_encoder_new();
    if (encoder == NULL) {
        Serial.println("Failed to create FLAC encoder instance.");
        inputFile.close();
        outputFile.close();
        return 0;
    }

    // Set encoder parameters
    FLAC__stream_encoder_set_channels(encoder, EXPECTED_CHANNELS);
    FLAC__stream_encoder_set_bits_per_sample(encoder, EXPECTED_BITS_PER_CHANNEL);
    FLAC__stream_encoder_set_sample_rate(encoder, EXPECTED_SAMPLE_RATE);
    FLAC__stream_encoder_set_compression_level(encoder, 5); // 0 (fastest) to 8 (best compression)
    // FLAC__stream_encoder_set_blocksize(encoder, 2048); // You might need to set a specific block size for memory reasons

    // Initialize the encoder with the write callback
    FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(
        encoder,
        write_callback,
        NULL, // No seek callback (for simplicity, we write linearly)
        NULL, // No tell callback
        NULL, // No metadata callback
        NULL  // No client data
    );

    if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
        Serial.printf("FLAC encoder initialization failed: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
        FLAC__stream_encoder_delete(encoder);
        inputFile.close();
        outputFile.close();
        return 0;
    }

    // Serial.println("FLAC encoder initialized. Starting encoding...");

    // Buffer for FLAC processing
    // For 16-bit signed, each sample is FLAC__int32
    // each PCM 16-bit signed has to be converted to FLAC__int32
    // this buffer should be 2x larger than wav_buffer that reads data
    FLAC__int32 interleaved_buffer[READ_BUFFER_SAMPLES]; // Single buffer for interleaved data
    
    // same memory used as buffer for reading WAV data
    int16_t *wav_buffer = (int16_t *)interleaved_buffer;

    uint32_t total_frames_encoded = 0;
    size_t bytes_read;
    const size_t bytes_per_channel = EXPECTED_BITS_PER_CHANNEL / 8;
    const size_t bytes_per_frame   = EXPECTED_CHANNELS * bytes_per_channel;
    // Serial.printf("bytes_per_channel %d\n", bytes_per_channel);
    // Serial.printf("bytes_per_frame   %d\n", bytes_per_frame);

    while (inputFile.available()) {
        bytes_read = inputFile.read((uint8_t *)wav_buffer, READ_BUFFER_BYTES);
        uint32_t frames_to_process = bytes_read / bytes_per_frame;

        if (frames_to_process == 0) {
            break; // No more full frames to process
        }
        // Serial.printf("frames to process %d\n", frames_to_process);

        // loop that converts 16-bit WAV PCM to 32-bit FLAC
        // run backwards from last to first sample
        for(int i = READ_BUFFER_SAMPLES-1; i >= 0; i--)
          interleaved_buffer[i] = wav_buffer[i];

        // Process interleaved audio data
        if (!FLAC__stream_encoder_process_interleaved(encoder, interleaved_buffer, frames_to_process)) {
            Serial.printf("FLAC encoding error: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
            break;
        }
        total_frames_encoded += frames_to_process;
    }
    // Serial.printf("Finished processing audio data. Total frames encoded: %lu\n", total_frames_encoded);

    int retval = 0;
    // Finalize the encoding process
    if (!FLAC__stream_encoder_finish(encoder)) {
        Serial.printf("FLAC encoder finish error: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
        retval = 0;
    } else {
        // Serial.println("FLAC encoding finished successfully!");
        retval = 1;
    }

    // Clean up
    FLAC__stream_encoder_delete(encoder);
    return retval; // success=1 fail==
}
