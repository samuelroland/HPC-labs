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

I'm not sure to understand why there is such a big difference between 12MB and 2MB. This cache is shared between all cores, so maybe we are always going beyond the L3 cache...
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

I'm going to take near the fastest as the average: **60000 Mbyte/s** for L3.


