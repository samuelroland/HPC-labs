# Rapport HPC Lab 5 - Profiling - Samuel Roland

## Ma machine

Extrait de l'ouput de `fastfetch`
```
OS: Fedora Linux 41 (KDE Plasma) x86_64
Host: 82RL (IdeaPad 3 17IAU7)
Kernel: Linux 6.13.5-200.fc41.x86_64
CPU: 12th Gen Intel(R) Core(TM) i7-1255U (12) @ 4.70 GHz
GPU: Intel Iris Xe Graphics @ 1.25 GHz [Integrated]
Memory: 15.34 GiB - DDR4
Swap: 8.00 GiB
```


## Partie 1 - Code de mesures de temperature
### create-sample
J'ai choisi une taille de 3000000 (3 millions) d'entrées en génération pour prendre assez de temps pour que les mesures de perf soient qualitatives mais que cela ne prenne non plus pas trop de temps durant les essais.
```sh
> ./build/create-sample 3000000
Created 3000000 measurements in 485.065000 ms
```

Cela génère un fichier `measurements.txt` de `40M` !

Les options de compilation du `Makefile` ont configuré `-O2`, on a donc déjà un première couche d'optimisation. J'ai ajouté `-ggdb` dans les flags comme conseillé dans le tuto, donc on a `-g -ggdb`. Regardons maintenant ce que perf peut nous apprendre.

Je vais mesurer l'événement `cpu-cycles` comme mesure du temps.
```sh
> sudo perf record --call-graph dwarf -e cpu-cycles ./build/create-sample 3000000
Created 3000000 measurements in 485.079000 ms
[ perf record: Woken up 63 times to write data ]
[ perf record: Captured and wrote 15.853 MB perf.data (1961 samples) ]
```

Ce qui nous donne ce flamegraph dans hotspot

![perf-one.png](imgs/perf-one.png)

On peut ignorer la partie tout en bas avec `_start` et jusqu'à `main` puisque cela inclue l'entièreté de notre programme, cela ne nous apprend rien. Juste au dessus, sans suprise au vu du code du `main()`, on trouve `fprintf`, `rand` et `rand_nd` les 3 fonctions utilisées dans la boucle principale.
```c
for (int i = 0; i < n; i++) {
    int c = rand() % ncities;
    double measurement = rand_nd(data[c].mean, 10);
    fprintf(fh, "%s;%.1f\n", data[c].city, measurement);
}
```
`fprintf` prend environ `70%` des cycles CPU. Cette fonction est évidemment bufferisée, il n'y a pas de syscall `write` à chaque appel heureusement, mais de combien est le buffer ?

Le fichier `stdio.h` sur ma machine donne cette taille de buffer par défaut.
```c
/* Default buffer size.  */
#define BUFSIZ 8192
```
oups, c'est super petit !

En tirant la commande du lien fourni [perf Examples](https://www.brendangregg.com/perf.html), j'ai trouvé cette commande intéressante pour mesurer le nombre de syscalls. On voit bien l'effet de la bufferisation, puisque 20 `fprintf` de plus ne change pas le nombre de syscalls, puisque le buffer interne est assez grand pour contenir 20 entrées de plus.
```sh
> sudo perf stat -e raw_syscalls:sys_enter -I 1000 ./build/create-sample 3000000
Created 3000000 measurements in 497.083000 ms
#           time             counts unit events
     0.499094570             10,153      raw_syscalls:sys_enter                                                
> sudo perf stat -e raw_syscalls:sys_enter -I 1000 ./build/create-sample 3000020
Created 3000020 measurements in 487.094000 ms
#           time             counts unit events
     0.489316098             10,153      raw_syscalls:sys_enter       
```

8192 bytes c'est largement `40*1024*1024/8192` 5120 fois plus petit que le total à stocker. Si on augmente cette taille ou on gère nous même notre propre buffer, on pourrait grandement diminuer le nombre de syscall de sauvegarde de ces résultats. Au lieu de 10,153 syscalls, on pourrait en avoir moins d'une centaine, avec un buffer beaucoup plus grand, comme il n'y a très peu de syscalls autour, le gain de performance sera net!

Par rapport au `rand()`, il y a probablement des meilleurs algorithmes d'aléatoire avec d'autres propriétés, ou des moyens de bidouiller pour faire un aléatoire avec des propriétés différentes. Par exemple, pour 3 millions d'éléments, comme en moyenne on va toucher chaque élément autant de fois, peut-être que les parcourir dans l'ordre sans aléatoire pour les 3/4 des tours pourraient être similaire en terme de probabilité ?

Une chose un peu étonnante aussi c'est pourquoi le -O2 n'a pas inliné la fonction `rand_nd`... peut-être à cause du static dont je ne comprends pas bien l'intérêt...
```c
double rand_nd(double mean, double stddev) {
```

J'ai pu le vérifier via `objdump`, il y a bien un `call` qui est fait.
```sh
> objdump -S ./build/create-sample
...
/home/sam/HEIG/year3/HPC/HPC-labs/lab05/code/create-sample.c:464
        double measurement = rand_nd(data[c].mean, 10);                                                                                                                     
  401199:       |  |  |   shl    rbx,0x4
  40119d:       |  |  |   vmovsd xmm0,QWORD PTR [rbx+0x4050a8]
  4011a5:       |  |  |   call   401320 <rand_nd>                
```


Avec ces accès à des villes random `data[c].mean` (avec c de random) je me demandais s'il n'y avait pas un soucis au niveau des cach-misses. Après un rapide calcul, il se trouve que la struct pour chaque vill fait 16 bytes et que `16*413= 6608 bytes` ce qui est largement inférieur au 32KB de cache L1 disponible.

Cette hypothèse a pu être vérifiée à l'aide des compteurs de référence de L1 et de référence manquées sur L1. On voit qu'il n'y a que **0.14%** de miss, ce qui indique que le cache L1 est très vite chargé et ces accès aléatoires n'ont pas du tout d'impact sur la performance.
```sh
> sudo perf stat -e L1-dcache-loads,L1-dcache-load-misses ./build/create-sample 3000000
Created 3000000 measurements in 482.196000 ms

Performance counter stats for './build/create-sample 3000000':

    883,230,295      cpu_atom/L1-dcache-loads/                                             (1.36%)
  1,674,071,426      cpu_core/L1-dcache-loads/                                             (98.64%)
<not supported>      cpu_atom/L1-dcache-load-misses/ 
      2,320,184      cpu_core/L1-dcache-load-misses/  #  0.14% of all L1-dcache accesses   (98.64%)
```
---

Valgrind ne détecte pas de fuite de mémoire.
```sh
> valgrind ./build/create-sample 3000000
==402997== Memcheck, a memory error detector
==402997== Copyright (C) 2002-2024, and GNU GPL'd, by Julian Seward et al.
==402997== Using Valgrind-3.24.0 and LibVEX; rerun with -h for copyright info
==402997== Command: ./build/create-sample 3000000
==402997== 
Created 3000000 measurements in 16492.614000 ms
==402997== 
==402997== HEAP SUMMARY:
==402997==     in use at exit: 0 bytes in 0 blocks
==402997==   total heap usage: 3 allocs, 3 frees, 5,592 bytes allocated
==402997== 
==402997== All heap blocks were freed -- no leaks are possible
==402997== 
==402997== For lists of detected and suppressed errors, rerun with: -s
==402997== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

```sh
valgrind --tool=callgrind  ./build/create-sample 3000000
```

En affichant seulement le nombre d'instructions executées, cette fois-ci on s'intéresse au nombre d'instructions executées et non aux cycles CPU passés, puisque Valgrind fait tourner notre code dans un "émulateur" de CPU, les mesures sont un peu différentes. Cela confirme les proportions de `fprintf` (85.6% proche de 70%) et de `rand_nd` et `rand`.
```sh
> callgrind_annotate --auto=yes --sort=Ir --show=Ir
1,183,238 ( 0.13%)      for (int i = 0; i < n; i++) {
4,338,536 ( 0.49%)          int c = rand() % ncities;
22,468,759 ( 2.56%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/stdlib/rand.c:rand (394,412x)
      677 ( 0.00%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/elf/../sysdeps/x86_64/dl-trampoline.h:_dl_runtime_resolve_xsave (1x)
1,577,648 ( 0.18%)          double measurement = rand_nd(data[c].mean, 10);
94,152,695 (10.74%)  => create-sample.c:rand_nd (394,412x)
2,366,476 ( 0.27%)          fprintf(fh, "%s;%.1f\n", data[c].city, measurement);
750,484,526 (85.60%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/stdio-common/fprintf.c:fprintf (394,412x)
```

On obtient un total de **876,759,078** instructions executées au total.

Le détails de l'annotation sur `rand_nd` est moins évidente, je n'ai pas réussi à faire en sorte d'avoir des calculs de pourcentage uniquement pour cette fonction. Les appels des fonction cos et sin (`__sin_fma` et `__cos_fma`) prennent chacune autour de 2%, si on additionne les pourcents au dessus de 1, on arrive vers les 10% pour toute la fonction `rand_nd` (`94,152,695 (10.74%)  => create-sample.c:rand_nd (394,412x)`), mais chacune de ses parties est assez courte, en gardant les calculs actuels, il parait difficile d'améliorer quoi que ce soit comme le temps est très réparti, une meilleure fonction `cos` si cela existe qui irait 2 fois plus vite, ne ferai gagner que 1% du total...

```
1,577,648 ( 0.18%)  double rand_nd(double mean, double stddev) {
        .               static double U, V;
        .               static int phase = 0;
        .               double Z;
        .           
1,183,236 ( 0.13%)      if (phase == 0) {
1,577,648 ( 0.18%)          U = (rand() + 1.) / (RAND_MAX + 2.);
11,234,381 ( 1.28%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/stdlib/rand.c:rand (197,206x)
1,380,442 ( 0.16%)          V = rand() / (RAND_MAX + 1.);
11,234,380 ( 1.28%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/stdlib/rand.c:rand (197,206x)
2,958,098 ( 0.34%)          Z = sqrt(-2 * log(U)) * sin(2 * M_PI * V);
10,612,101 ( 1.21%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/math/./w_log_template.c:log@@GLIBC_2.29 (197,206x)
17,614,635 ( 2.01%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/math/../sysdeps/ieee754/dbl-64/s_sin.c:__sin_fma (197,206x)
    1,347 ( 0.00%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/elf/../sysdeps/x86_64/dl-trampoline.h:_dl_runtime_resolve_xsave (2x)
        .               } else {
2,760,888 ( 0.31%)          Z = sqrt(-2 * log(U)) * cos(2 * M_PI * V);
10,612,101 ( 1.21%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/math/./w_log_template.c:log@@GLIBC_2.29 (197,206x)
18,249,807 ( 2.08%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/math/../sysdeps/ieee754/dbl-64/s_sin.c:__cos_fma (197,206x)
      687 ( 0.00%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/elf/../sysdeps/x86_64/dl-trampoline.h:_dl_runtime_resolve_xsave (1x)
        .               }
1,183,236 ( 0.13%)      phase = 1 - phase;
        .           
  788,824 ( 0.09%)      return Z * stddev + mean;
1,183,236 ( 0.13%)  }
```

Je sais pas si ça se fait avec des fonctions de la libc, mais on pourrait aussi essayer d'inliner ces fonctions de math.

### Conclusion create-sample
Pour améliorer ce code, j'aurai fait dans l'ordre
1. Inliner à la main ou tester l'effet du mot clé `inline` sur `rand_nd`
1. Utiliser un buffer beaucoup plus grand pour stocker les résultats à inscrire dans le fichier de sortie pour limiter un maximum les syscalls effectués pour write le fichier
1. Utiliser un autre algorithme d'aléatoire
1. Peut-être paralliser la génération pour découper le nombre total d'entrée à générer en plusieurs morceaux

### analyze
Pour gagner un peu de temps de recherche, je ne prends que 1 millions d'entrées.
```sh
> ./build/create-sample 1000000
```

Ce qui nous donne environ 488ms d'exécution.
```sh
> hyperfine ./build/analyze
Benchmark 1: ./build/analyze
  Time (mean ± σ):     488.9 ms ±  64.9 ms    [User: 485.2 ms, System: 2.8 ms]
  Range (min … max):   331.2 ms … 546.9 ms    10 runs
```

## Partie 2 - DTMF

