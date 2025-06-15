// This code uses fftw3 library to detect the 2 most important frequencies in a given float buffer of a given size
// This code is coming from Copilot, with Claude Sonnet 3.5 model, as I have no idea on how to do it myself
#include <fftw3.h>
#include <math.h>

// Input: float buffer[], int length, float sample_rate
// Returns: the 2 main frequencies found
void find_main_frequencies(const float *buffer, int length, float sample_rate, float *freq1, float *freq2) {
    double *in = fftw_malloc(sizeof(double) * length);
    fftw_complex *out = fftw_malloc(sizeof(fftw_complex) * (length / 2 + 1));

    // Copy input buffer to doubles
    for (int i = 0; i < length; i++) {
        in[i] = (double) buffer[i];
    }

    fftw_plan p = fftw_plan_dft_r2c_1d(length, in, out, FFTW_ESTIMATE);
    fftw_execute(p);

    // Find 2 highest magnitudes
    double max1 = 0, max2 = 0;
    int idx1 = 0, idx2 = 0;

    for (int i = 1; i < length / 2 + 1; i++) {
        double mag = sqrt(out[i][0] * out[i][0] + out[i][1] * out[i][1]);
        if (mag > max1) {
            max2 = max1;
            idx2 = idx1;
            max1 = mag;
            idx1 = i;
        } else if (mag > max2) {
            max2 = mag;
            idx2 = i;
        }
    }

    *freq1 = idx1 * sample_rate / length;
    *freq2 = idx2 * sample_rate / length;

    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
}
