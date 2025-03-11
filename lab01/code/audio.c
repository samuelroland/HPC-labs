#include "const.h"
#include <sndfile-64.h>
#include <sndfile.h>
#include <stdlib.h>
// Help: https://libsndfile.github.io/libsndfile/api.html#open

// Note: These functions reuse some code provided in libsndfile.c

int write_wav_file(const char *filename, const float *freqs, sf_count_t samples_count) {
    SF_INFO sfinfo;
    // From docs: When opening a file for write, the caller must fill in structure members samplerate, channels, and format.
    sfinfo.samplerate = SAMPLE_RATE;
    sfinfo.channels = 1;// TODO: check that
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    SNDFILE *outfile = sf_open(filename, SFM_WRITE, &sfinfo);
    if (!outfile) {
        fprintf(stderr, "Erreur: Impossible de créer le fichier '%s': %s\n", filename, sf_strerror(NULL));
        return 1;
    }

    sf_writef_float(outfile, freqs, samples_count);

    sf_close(outfile);
    return 0;
}

int read_wav_file(const char *filename, float **freqs, sf_count_t *samples_count) {
    SF_INFO sfinfo;
    SNDFILE *infile = sf_open(filename, SFM_READ, &sfinfo);
    if (!infile) {
        fprintf(stderr, "Erreur: Impossible d'ouvrir le fichier '%s': %s\n", filename, sf_strerror(NULL));
        return 1;
    }

    SF_FORMAT_INFO format_info;
    format_info.format = sfinfo.format;
    sf_command(infile, SFC_GET_FORMAT_INFO, &format_info, sizeof(format_info));

    printf("Informations sur le fichier '%s':\n", filename);
    printf("  - Format : %08x  %s %s\n", format_info.format, format_info.name, format_info.extension);
    printf("  - Nombre de canaux : %d\n", sfinfo.channels);
    printf("  - Fréquence d'échantillonnage : %d Hz\n", sfinfo.samplerate);
    printf("  - Nombre d'échantillons : %lld\n", (long long) sfinfo.frames);
    printf("  - Durée : %.2f secondes\n", (double) sfinfo.frames / sfinfo.samplerate);

    if ((sfinfo.format & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
        fprintf(stderr, "Erreur: Le fichier n'est pas au format WAV.\n");
        sf_close(infile);
        return 1;
    }

    *freqs = (float *) malloc(sfinfo.frames * sfinfo.channels * sizeof(float));
    if (!freqs) {
        fprintf(stderr, "Erreur: Impossible d'allouer de la mémoire.\n");
        sf_close(infile);
        return 1;
    }

    sf_count_t frames_read = sf_readf_float(infile, *freqs, sfinfo.frames);
    if (frames_read != sfinfo.frames) {
        fprintf(stderr, "Avertissement: Seuls %lld frames sur %lld ont été lus.\n",
                (long long) frames_read, (long long) sfinfo.frames);
    }

    sf_close(infile);

    *samples_count = sfinfo.frames;

    return 0;
}
