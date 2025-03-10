#include "const.h"
#include <sndfile-64.h>
#include <sndfile.h>
// Help: https://libsndfile.github.io/libsndfile/api.html#open

// Note: These functions reuse some code provided in libsndfile.c

void write_wav_file(const char *filename, const float *freqs, sf_count_t samples_count) {
    SF_INFO sfinfo;
    // From docs: When opening a file for write, the caller must fill in structure members samplerate, channels, and format.
    sfinfo.samplerate = SAMPLE_RATE;
    sfinfo.channels = 1;// TODO: check that
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    // TODO: should i define the sample rate somewhere ?
    SNDFILE *outfile = sf_open(filename, SFM_WRITE, &sfinfo);
    if (!outfile) {
        fprintf(stderr, "Erreur: Impossible de créer le fichier '%s': %s\n", filename, sf_strerror(NULL));
        return;
    }

    sf_writef_float(outfile, freqs, samples_count);

    sf_close(outfile);
}

void read_wav_file(const char *filename, const float *freqs) {
}
