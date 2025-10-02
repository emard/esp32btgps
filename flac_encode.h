// git clone https://github.com/pschatzmann/arduino-libflac
// git clone https://github.com/pschatzmann/codec-ogg
// zip -r arduino-libflac.zip arduino-libflac
// zip -r codec-ogg.zip codec-ogg
// Sketch -> Include Library -> Add .ZIP Library... -> arduino-libflac.zip and codec-ogg.zip

#include <SD_MMC.h>

// Define WAV header structure (simplified)
// This structure needs to match the actual header of input WAV file
typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
    char subchunk1ID[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char subchunk2ID[4];
    uint32_t subchunk2Size;
} wav_header_t;

// Assuming 6 channels, 1000 Hz, 16-bit signed
const uint32_t EXPECTED_CHANNELS = 6;
const uint32_t EXPECTED_SAMPLE_RATE = 1000; // Hz
const uint32_t EXPECTED_BITS_PER_CHANNEL = 16; // bits

// one frame contains samples for all channels for one sample rate time step (1 ms)
// here is defined read buffer size which is limited by
// CPU SD_MMC DMA read size limit and not by available RAM
// too large value will crash CPU at inputFile.read(wav_buffer, READ_BUFFER_BYTES)
const size_t READ_BUFFER_FRAMES = 200; // max is around 220-240, don't come close to it
const size_t READ_BUFFER_SAMPLES = READ_BUFFER_FRAMES * EXPECTED_CHANNELS;
const size_t READ_BUFFER_BYTES = READ_BUFFER_SAMPLES * EXPECTED_BITS_PER_CHANNEL / 8; // in bytes

// open() files before flac_encode()
// close() files after flac_encode()
// speed of FLAC necoding on SD_MMC card is about 300 K/s
int flac_encode(File &flacOutputFile, File &wavInputFile);
