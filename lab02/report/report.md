# HPC Lab 2 - Report
Author: Samuel Roland

## My code

Setup [Likwid](https://github.com/RRZE-HPC/likwid/) too. For this lab, I maintain both `CMakeLists.txt` and `xmake.lua`, but I use xmake generated binaries in my report.

### Via CMake
Setup the `fftw` library and `libsnd`, on Fedora here are the DNF packages
```sh
sudo dnf install fftw fftw-devel libsndfile-devel
```
Setup [Likwid](https://github.com/RRZE-HPC/likwid/) too.

Compile
```sh
cmake . -Bbuild && cmake --build build/ -j 8
```

And run the buffers variant
```sh
./build/dtmf_encdec-buffers decode trivial-alphabet.wav
```
Or run the fft variant
```sh
./build/dtmf_encdec-fft decode trivial-alphabet.wav
```

### Via Xmake
Install [xmake](https://xmake.io/#/getting_started)
```sh
curl -fsSL https://xmake.io/shget.text | bash
```
or via DNF
```sh
sudo dnf install xmake
```

Build the code, it will prompt you to install fft and libsnd dependencies on a global xmake cache
```sh
xmake
```

And run the buffers variant
```sh
xmake run dtmf_encdec-buffers decode trivial-alphabet.wav
# or directly
./build/linux/x86_64/release/dtmf_encdec-buffers decode trivial-alphabet.wav
```
Or run the fft variant
```sh
xmake run dtmf_encdec-fft decode trivial-alphabet.wav
# or directly
./build/linux/x86_64/release/dtmf_encdec-fft decode trivial-alphabet.wav
```

## My machine
I'm working on my tour at home, because my laptop is not supported by likwid because Intel 12th Gen is not supported currently.

Extract from `fastfetch`
```
OS: Fedora Linux 41 (KDE Plasma) x86_64
Host: XPS 8930 (1.1.21)
Kernel: Linux 6.13.5-200.fc41.x86_64
CPU: Intel(R) Core(TM) i7-8700 (12) @ 4.60 GHz
GPU 1: NVIDIA GeForce GTX 1060 6GB [Discrete]
GPU 2: Intel UHD Graphics 630 @ 1.20 GHz [Integrated]
Memory: 15.32 GiB - DDR4
Swap: 8.00 GiB
Disk: 236.87 GiB - btrfs
```

![lstopo-sx](./imgs/lstopo-sx.png)

```
> likwid-topology 
--------------------------------------------------------------------------------
CPU name:	Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz
CPU type:	Intel Coffeelake processor
CPU stepping:	10
********************************************************************************
Hardware Thread Topology
********************************************************************************
Sockets:		1
Cores per socket:	6
Threads per core:	2
--------------------------------------------------------------------------------
HWThread        Thread        Core        Die        Socket        Available
0               0             0           0          0             *                
1               0             1           0          0             *                
2               0             2           0          0             *                
3               0             3           0          0             *                
4               0             4           0          0             *                
5               0             5           0          0             *                
6               1             0           0          0             *                
7               1             1           0          0             *                
8               1             2           0          0             *                
9               1             3           0          0             *                
10              1             4           0          0             *                
11              1             5           0          0             *                
--------------------------------------------------------------------------------
Socket 0:		( 0 6 1 7 2 8 3 9 4 10 5 11 )
--------------------------------------------------------------------------------
********************************************************************************
Cache Topology
********************************************************************************
Level:			1
Size:			32 kB
Cache groups:		( 0 6 ) ( 1 7 ) ( 2 8 ) ( 3 9 ) ( 4 10 ) ( 5 11 )
--------------------------------------------------------------------------------
Level:			2
Size:			256 kB
Cache groups:		( 0 6 ) ( 1 7 ) ( 2 8 ) ( 3 9 ) ( 4 10 ) ( 5 11 )
--------------------------------------------------------------------------------
Level:			3
Size:			12 MB
Cache groups:		( 0 6 1 7 2 8 3 9 4 10 5 11 )
--------------------------------------------------------------------------------
********************************************************************************
NUMA Topology
********************************************************************************
NUMA domains:		1
--------------------------------------------------------------------------------
Domain:			0
Processors:		( 0 6 1 7 2 8 3 9 4 10 5 11 )
Distances:		10
Free memory:		1893.86 MB
Total memory:		15689.5 MB
--------------------------------------------------------------------------------
```

## Roofline
I'm following the [Tutorial: Empirical Roofline Model](https://github.com/RRZE-HPC/likwid/wiki/Tutorial%3A-Empirical-Roofline-Model) 

```
> likwid-bench -p
Number of Domains 5
Domain 0:
	Tag N: 0 6 1 7 2 8 3 9 4 10 5 11
Domain 1:
	Tag S0: 0 6 1 7 2 8 3 9 4 10 5 11
Domain 2:
	Tag D0: 0 6 1 7 2 8 3 9 4 10 5 11
Domain 3:
	Tag C0: 0 6 1 7 2 8 3 9 4 10 5 11
Domain 4:
	Tag M0: 0 6 1 7 2 8 3 9 4 10 5 11
```

My machine has one physical socket, and I'm going to take domain 0 as recommended by the teacher, so with tag `N`.

Before starting the maxperf and maxband search I tried to know how much memory is eating

I know that my program is using more memory than what's available
```sh
/usr/bin/time -v ./build/linux/x86_64/release/dtmf_encdec-buffers decode $PWD/short.wav
```

### Searching for maxperf
We want to skip the first 0 and 1 hardware cores used by the OS, so we pin the program on the core number 2 with `taskset -c 2`, my program is single-threaded. We run the peakflops_sp (single precision because my code only use `float` and no `double`). Using 32KB because this is the L1 data cache size, we don't want to go further because we don't want our measures to be influenced by cache misses as we are not calculating the memory bandwidth here. We want to use only 1 thread in the benchmark with `:1`

```sh
taskset -c 2 likwid-bench -t peakflops_sp -W N:32kB:1
```
```
Allocate: Process running on hwthread 2 (Domain N) - Vector length 8000/32000 Offset 0 Alignment 1024
--------------------------------------------------------------------------------
LIKWID MICRO BENCHMARK
Test: peakflops_sp
--------------------------------------------------------------------------------
Using 1 work groups
Using 1 threads
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
Group: 0 Thread 0 Global Thread 0 running on hwthread 2 - Vector length 8000 Offset 0
--------------------------------------------------------------------------------
Cycles:			4756395192
CPU Clock:		2610062016
Cycle Clock:		2610062016
Time:			1.822330e+00 sec
Iterations:		131072
Iterations per thread:	131072
Inner loop executions:	8000
Size (Byte):		32000
Size per thread:	32000
Number of Flops:	16777216000
MFlops/s:		9206.46
Data volume (Byte):	4194304000
MByte/s:		2301.62
Cycles per update:	4.536052
Cycles per cacheline:	72.576831
Loads per update:	1
Stores per update:	0
Load bytes per element:	4
Store bytes per elem.:	0
Instructions:		20971520032
UOPs:			19922944000
--------------------------------------------------------------------------------
```

The interesting line is `MFlops/s: 9206.46`, so we can round the `maxperf` to **9206 MFlops/s**

### Searching for maxband
CPU cache sizes for core 2
- L1: 48KB
- L2: 1280KB
- L3: 12MB

Let's start with L1, I chose the `copy` benchmark and I use a vector of `48KB` at it is the cache size.
<!--TODO: why copy and not load ?-->
```sh
> taskset -c 2 likwid-bench -t copy -W N:48KB:1
Allocate: Process running on hwthread 2 (Domain N) - Vector length 3000/24000 Offset 0 Alignment 512
Allocate: Process running on hwthread 2 (Domain N) - Vector length 3000/24000 Offset 0 Alignment 512
--------------------------------------------------------------------------------
LIKWID MICRO BENCHMARK
Test: copy
--------------------------------------------------------------------------------
Using 1 work groups
Using 1 threads
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
Group: 0 Thread 0 Global Thread 0 running on hwthread 2 - Vector length 3000 Offset 0
--------------------------------------------------------------------------------
Cycles:			3882628234
CPU Clock:		2611024867
Cycle Clock:		2611024867
Time:			1.487013e+00 sec
Iterations:		4194304
Iterations per thread:	4194304
Inner loop executions:	750
Size (Byte):		48000
Size per thread:	48000
Number of Flops:	0
MFlops/s:		0.00
Data volume (Byte):	201326592000
MByte/s:		135389.92
Cycles per update:	0.308564
Cycles per cacheline:	2.468509
Loads per update:	1
Stores per update:	1
Load bytes per element:	8
Store bytes per elem.:	8
Load/store ratio:	1.00
Instructions:		34603008016
UOPs:			44040192000
--------------------------------------------------------------------------------
```
The line `MByte/s:		135389.92` gives us the maxband for L1 cache. 
```sh
> taskset -c 2 likwid-bench -t copy -W N:48KB:1 | grep MByte
MByte/s:		135454.60
> taskset -c 2 likwid-bench -t copy -W N:10KB:1 | grep MByte
MByte/s:		138042.56
```

As expected, going further that 48KB means the L1 cache is not sufficient, the L2 takes the relay and we see a massive bandwidth reduction.
```sh
> taskset -c 2 likwid-bench -t copy -W N:50KB:1 | grep MByte
MByte/s:		108108.22
> taskset -c 2 likwid-bench -t copy -W N:49KB:1 | grep MByte
MByte/s:		123534.99
> taskset -c 2 likwid-bench -t copy -W N:53KB:1 | grep MByte
MByte/s:		102391.57
> taskset -c 2 likwid-bench -t copy -W N:80KB:1 | grep MByte
MByte/s:		97608.80
> taskset -c 2 likwid-bench -t copy -W N:80KB:1 | grep MByte
MByte/s:		98218.39
```

Going below 48KB can be faster sometimes.
```sh
> taskset -c 2 likwid-bench -t copy -W N:40KB:1 | grep MByte
MByte/s:		146047.24
> taskset -c 2 likwid-bench -t copy -W N:20KB:1 | grep MByte
MByte/s:		142454.28
> taskset -c 2 likwid-bench -t copy -W N:10KB:1 | grep MByte
MByte/s:		137338.58
```

I'm going to take **135000 MByte/s** for L1 as a vague average of these results <= 48KB.

### For L2 cache

Same strategy as before, trying the maximum of `1280KB`

```sh
taskset -c 2 likwid-bench -t copy -W N:1280KB:1
```
```
Allocate: Process running on hwthread 2 (Domain N) - Vector length 80000/640000 Offset 0 Alignment 512
Allocate: Process running on hwthread 2 (Domain N) - Vector length 80000/640000 Offset 0 Alignment 512
--------------------------------------------------------------------------------
LIKWID MICRO BENCHMARK
Test: copy
--------------------------------------------------------------------------------
Using 1 work groups
Using 1 threads
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
Group: 0 Thread 0 Global Thread 0 running on hwthread 2 - Vector length 80000 Offset 0
--------------------------------------------------------------------------------
Cycles:			2772963680
CPU Clock:		2611130390
Cycle Clock:		2611130390
Time:			1.061978e+00 sec
Iterations:		65536
Iterations per thread:	65536
Inner loop executions:	20000
Size (Byte):		1280000
Size per thread:	1280000
Number of Flops:	0
MFlops/s:		0.00
Data volume (Byte):	83886080000
MByte/s:		78990.39
Cycles per update:	0.528901
Cycles per cacheline:	4.231207
Loads per update:	1
Stores per update:	1
Load bytes per element:	8
Store bytes per elem.:	8
Load/store ratio:	1.00
Instructions:		14417920016
UOPs:			18350080000
--------------------------------------------------------------------------------
```


We get `MByte/s:		78990.39`, but further executions give us $\pm 2000$ around that...

Going with lower values between 48KB and 1280KB gives us more stable results, I guess this is because some other software are running on this core too, the benchmark is not the only one filling this cache.

```sh
> taskset -c 2 likwid-bench -t copy -W N:800KB:1 | grep MByte
MByte/s:		98294.43
> taskset -c 2 likwid-bench -t copy -W N:400KB:1 | grep MByte
MByte/s:		99025.11
> taskset -c 2 likwid-bench -t copy -W N:200KB:1 | grep MByte
MByte/s:		98297.40
> taskset -c 2 likwid-bench -t copy -W N:100KB:1 | grep MByte
MByte/s:		98239.87
> taskset -c 2 likwid-bench -t copy -W N:70KB:1 | grep MByte
MByte/s:		98103.01
```
I'm taking a rough average of **98200 MByte/s** for L2.

### L3
I find that pretty strange that several execution of small vector of `1MB` gives us very different results...
```sh
> taskset -c 2 likwid-bench -t copy -W N:1MB:1 | grep MByte
MByte/s:		83870.75
> taskset -c 2 likwid-bench -t copy -W N:1MB:1 | grep MByte
MByte/s:		90445.30
> taskset -c 2 likwid-bench -t copy -W N:1MB:1 | grep MByte
MByte/s:		88562.67
```

```sh
> taskset -c 2 likwid-bench -t copy -W N:12MB:1 | grep MByte
MByte/s:		42956.49
> taskset -c 2 likwid-bench -t copy -W N:10MB:1 | grep MByte
MByte/s:		51069.79
> taskset -c 2 likwid-bench -t copy -W N:8MB:1 | grep MByte
MByte/s:		56306.97
> taskset -c 2 likwid-bench -t copy -W N:4MB:1 | grep MByte
MByte/s:		58439.60
> taskset -c 2 likwid-bench -t copy -W N:4MB:1 | grep MByte
MByte/s:		58671.92
> taskset -c 2 likwid-bench -t copy -W N:3MB:1 | grep MByte
MByte/s:		58788.55
> taskset -c 2 likwid-bench -t copy -W N:2MB:1 | grep MByte
MByte/s:		61931.24
> taskset -c 2 likwid-bench -t copy -W N:2MB:1 | grep MByte
MByte/s:		61202.62
```

## Note pour la suite
attention à bien adapter le roofline en fonction du programme qu'on va faire et aux flags d'optimisations.
attention à bien être en -O0 pour le début pour être sûr que il n'y a pas des trucs optimisations liés à avx ou sse qui s'activent
si on utilise que des float -> faire du single precision `sp` !

