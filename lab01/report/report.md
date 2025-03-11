# HPC Lab 1 - Report
Author: Samuel Roland

## My machine

Extract from `fastfetch`
```
OS: Fedora Linux 41 (KDE Plasma) x86_64
Host: 82RL (IdeaPad 3 17IAU7)
Kernel: Linux 6.13.5-200.fc41.x86_64
CPU: 12th Gen Intel(R) Core(TM) i7-1255U (12) @ 4.70 GHz
GPU: Intel Iris Xe Graphics @ 1.25 GHz [Integrated]
Memory: 15.34 GiB - DDR4
Swap: 8.00 GiB
```

## Setup

Setup the `fftw` library and `libsnd`, on Fedora here are the DNF packages
```sh
sudo dnf install fftw fftw-devel libsndfile-devel
```

Compile
```sh
cmake . -Bbuild && cmake --build build/ -j 8
```

And run
```sh
./build/dtmf_encdec decode trivial-alphabet.wav
```

## Approach
I did 2 variants of the decoder:
1. **Variant 1 - Tone buffers comparisons**: This first one is trying to compare raw float buffers out of the formula with a chunk of audio read from disk, and take the button that has the lowest score. The score is the average of absolute differences between the 2 buffers.
1. **Variant 2 - FFT based frequence extraction**: This one uses `fftw` to extract the main frequencies and take the button behind this pair if this is proximate enough to a known pair.

It's possible to switch between these 2 variants with this flag in `decoder.h`

```c
#define DECODER_VARIANT 1// 1 is float buffer comparison, 2 is via fft
```

## Measures
### Encoder
Considering this text
```sh
echo -n "0123456789abcdefghijklmnopqrstuvwxyz.!?" > input.txt
```

```sh
> hyperfine './build/dtmf_encdec encode input.txt output.wav'
Benchmark 1: ./build/dtmf_encdec encode input.txt output.wav
  Time (mean ± σ):      12.9 ms ±   3.8 ms    [User: 6.4 ms, System: 4.9 ms]
  Range (min … max):     8.6 ms …  22.4 ms    128 runs
```

**12.9ms**
 
### Decoder variant 1
Given the previous `output.wav`

```sh
> hyperfine "./build/dtmf_encdec decode output.wav"
Benchmark 1: ./build/dtmf_encdec decode output.wav
  Time (mean ± σ):      29.3 ms ±   4.0 ms    [User: 24.1 ms, System: 5.0 ms]
  Range (min … max):    23.5 ms …  43.4 ms    82 runs
```

Considering a longer wav generated with this string
```sh
echo -n "0123456789abcdefghijklmnopqrstuvwxyz.!? 123123123156723484325287934892739423726578614789234892349825624242928934892348924820489234928048298492856523472374023784923492834982948234023242234242287432738472374892379842738947298356237645023744232342842848894280420sfbkadfkjlwejltngijooiuwerjzniodfsakjhjhwerhkjwrjkhaflkjasdkjhsadgjbawlrwjlkaslkjd" > input.txt
```

```sh
> hyperfine "./build/dtmf_encdec decode long.wav"
Benchmark 1: ./build/dtmf_encdec decode long.wav
  Time (mean ± σ):     126.3 ms ±   2.9 ms    [User: 109.2 ms, System: 16.5 ms]
  Range (min … max):   122.8 ms … 136.2 ms    23 runs
```
 
### Decoder variant 2

Given the previous `output.wav`

```sh
> hyperfine "./build/dtmf_encdec decode output.wav"
Benchmark 1: ./build/dtmf_encdec decode output.wav
  Time (mean ± σ):      28.7 ms ±   3.6 ms    [User: 24.3 ms, System: 4.3 ms]
  Range (min … max):    23.1 ms …  43.2 ms    112 runs
 
```

Considering a longer wav generated with this string
```sh
echo -n "0123456789abcdefghijklmnopqrstuvwxyz.!? 123123123156723484325287934892739423726578614789234892349825624242928934892348924820489234928048298492856523472374023784923492834982948234023242234242287432738472374892379842738947298356237645023744232342842848894280420sfbkadfkjlwejltngijooiuwerjzniodfsakjhjhwerhkjwrjkhaflkjasdkjhsadgjbawlrwjlkaslkjd" > input.txt
```

```sh
> hyperfine "./build/dtmf_encdec decode long.wav"
Benchmark 1: ./build/dtmf_encdec decode long.wav
  Time (mean ± σ):     127.9 ms ±   4.5 ms    [User: 109.9 ms, System: 17.6 ms]
  Range (min … max):   122.7 ms … 145.3 ms    23 runs
```

## Comparison
My 2 variant are strangely very near (`29.3` and `28.7`, `126.3 ms` and `127.9 ms`), I thought the FFT would be slower but that's not the case... The range is bigger. I don't know how I can analyse this further actually...
