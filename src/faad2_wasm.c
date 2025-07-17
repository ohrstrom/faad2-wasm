#include <emscripten/emscripten.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAIN_DEC 1
#define SBR_DEC 1
#define PS_DEC 1

#include "neaacdec.h"
#include "faad.h"

static NeAACDecHandle handle;

EMSCRIPTEN_KEEPALIVE
unsigned long get_faad_capabilities() {
    return NeAACDecGetCapabilities();
}


EMSCRIPTEN_KEEPALIVE
int init_decoder(const unsigned char *asc, int asc_len) {
    handle = NeAACDecOpen();
    if (!handle) return -1;

    NeAACDecConfigurationPtr config = NeAACDecGetCurrentConfiguration(handle);

    config->outputFormat = FAAD_FMT_FLOAT;
    config->defSampleRate = 48000;

    if (!NeAACDecSetConfiguration(handle, config)) {
        return -2;
    }

    unsigned long samplerate = 0;
    unsigned char channels = 0;

    if (NeAACDecInit2(handle, (unsigned char *)asc, asc_len, &samplerate, &channels) < 0) {
        return -3;
    }

    return 0;
}

EMSCRIPTEN_KEEPALIVE
int decode_frame(const unsigned char *data, int len, void *out, int out_size) {
    NeAACDecFrameInfo info;
    void *decoded = NeAACDecDecode(handle, &info, (unsigned char *)data, len);

    if (!decoded || info.error > 0 || !info.samples || info.samples * sizeof(float) > out_size) {
        return -1;
    }

    memcpy(out, decoded, info.samples * sizeof(float));

    uint32_t samplerate_index = 0;
    switch (info.samplerate) {
        case 8000: samplerate_index = 1; break;
        case 16000: samplerate_index = 2; break;
        case 22050: samplerate_index = 3; break;
        case 32000: samplerate_index = 4; break;
        case 44100: samplerate_index = 5; break;
        case 48000: samplerate_index = 6; break;
        case 64000: samplerate_index = 7; break;
        case 88200: samplerate_index = 8; break;
        case 96000: samplerate_index = 9; break;
        default: samplerate_index = 0; break;
    }

    uint32_t packed = 0;
    packed |= (samplerate_index & 0xF) << 28;
    packed |= (info.channels & 0xF) << 24;
    packed |= (info.samples & 0xFFFFFF);

    return packed;
}
