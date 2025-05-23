
lab machine with Haswell architecture

```
> likwid-topology
--------------------------------------------------------------------------------
CPU name:       Intel(R) Core(TM) i7-4770 CPU @ 3.40GHz
CPU type:       Intel Core Haswell processor
CPU stepping:   3
********************************************************************************
Hardware Thread Topology
********************************************************************************
Sockets:                1
CPU dies:               1
Cores per socket:       4
Threads per core:       2
--------------------------------------------------------------------------------
HWThread        Thread        Core        Die        Socket        Available
0               0             0           0          0             *                
1               0             1           0          0             *                
2               0             2           0          0             *                
3               0             3           0          0             *                
4               1             0           0          0             *                
5               1             1           0          0             *                
6               1             2           0          0             *                
7               1             3           0          0             *                
--------------------------------------------------------------------------------
Socket 0:               ( 0 4 1 5 2 6 3 7 )
--------------------------------------------------------------------------------
********************************************************************************
Cache Topology
********************************************************************************
Level:                  1
Size:                   32 kB
Cache groups:           ( 0 4 ) ( 1 5 ) ( 2 6 ) ( 3 7 )
--------------------------------------------------------------------------------
Level:                  2
Size:                   256 kB
Cache groups:           ( 0 4 ) ( 1 5 ) ( 2 6 ) ( 3 7 )
--------------------------------------------------------------------------------
Level:                  3
Size:                   8 MB
Cache groups:           ( 0 4 1 5 2 6 3 7 )
--------------------------------------------------------------------------------
********************************************************************************
NUMA Topology
********************************************************************************
NUMA domains:           1
--------------------------------------------------------------------------------
Domain:                 0
Processors:             ( 0 4 1 5 2 6 3 7 )
Distances:              10
Free memory:            1786.11 MB
Total memory:           15928.3 MB
--------------------------------------------------------------------------------
```

```
> likwid-bench -p

Number of Domains 5
Domain 0:
        Tag N: 0 4 1 5 2 6 3 7
Domain 1:
        Tag S0: 0 4 1 5 2 6 3 7
Domain 2:
        Tag D0: 0 4 1 5 2 6 3 7
Domain 3:
        Tag C0: 0 4 1 5 2 6 3 7
Domain 4:
        Tag M0: 0 4 1 5 2 6 3 7
```

