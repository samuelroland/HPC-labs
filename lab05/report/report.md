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

8192 bytes c'est largement `40*1024*1024/8192` 5120 fois plus petit que le total à stocker. Si on augmente cette taille ou on gère nous même notre propre buffer, on pourrait grandement diminuer le nombre de syscall de sauvegarde de ces résultats. Au lieu de 10,153 syscalls, on pourrait en avoir une dizaine, avec un buffer beaucoup plus grand, comme il n'y a très peu de syscalls autour, le gain de performance sera net! J'imagine que le cout d'allocation de 40MB ou une fraction de ceci, est moins chère que 10000 syscalls en trop.

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
...
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

Les détails de l'annotation sur `rand_nd` est moins évidente, je n'ai pas réussi à faire en sorte d'avoir des calculs de pourcentage uniquement pour cette fonction. Les appels des fonction cos et sin (`__sin_fma` et `__cos_fma`) prennent chacune autour de 2%, si on additionne les pourcents au dessus de 1, on arrive vers les 10% pour toute la fonction `rand_nd` (`94,152,695 (10.74%)  => create-sample.c:rand_nd (394,412x)`). En gardant les calculs actuels, il parait difficile d'améliorer quoi que ce soit comme le temps est très réparti. Par exemple une meilleure fonction `cos` si cela existe qui irait 2 fois plus vite, ne ferai gagner que 1% du total...

```sh
> callgrind_annotate --auto=yes --sort=Ir --show=Ir
...
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

Je reprends les mêmes commandes qu'avant pour ce deuxième binaire.
```sh
> sudo perf record --call-graph dwarf -e cpu-cycles ./build/analyze
```

![perf-two.png](imgs/perf-two.png)

Il se trouve que... sans grande surprise `getcity` prend une proportion complètement énorme du programme (82%). Ce n'est pas étonnant car son implémentation recherche une ville par son nom en parcourant le tableau de résultats, nous sommes en $O(N)$ avec N le nombre de villes au total (= aussi au nombre de résultats du coup). Et ce `getcity` est appelé pour chaque ligne. Ainsi l'implémentation est en $O(N*M)$ si M est le nombre de ligne. On voit que le prochain étage sur le flamegraph c'est le `__strcmp_avx2` au dessus de `getcity`, c'est cette comparaison de string qui prend le plus de temps au total, normal puisqu'on compare beaucoup de de noms de villes avant de trouver la bonne.

A noter aussi le 3.74% de temps passé sur `fgets`, qui est appelé pour chaque ligne. Ce qui est très inefficace, on pourrait lire plusieurs lignes à la fois et juste déplacer notre pointeur avec la recherche du `;` sur le buffer. Le buffer a en plus une taille de 2^10 bytes, donc 1024 bytes pour stocker une cinquantaine de caractères... On pourrait largement en stocker moins.

```sh
valgrind --tool=callgrind ./build/analyze
```

Le code annoté confirme le temps important de `strcmp` dans `getcity`
```sh
> callgrind_annotate --auto=yes --sort=Ir --show=Ir
...
            .           static int getcity(const char *city, struct result results[], int nresults) {
  622,152,194 ( 8.51%)      for (int i = 0; i < nresults; i++) {
1,657,402,143 (22.68%)          if (strcmp(results[i].city, city) == 0) {
4,141,006,380 (56.66%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/string/../sysdeps/x86_64/multiarch/strcmp-avx2.S:__strcmp_avx2 (207,050,319x)
          720 ( 0.00%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/elf/../sysdeps/x86_64/dl-trampoline.h:_dl_runtime_resolve_xsave (1x)
            .                       return i;
            .                   }
            .               }
            .           
            .               return -1;
            .           }

```

On trouve aussi `strtod` le parsing du string representant un float vers un float.
```
...
    5,000,004 ( 0.07%)          double measurement = strtod(pos + 1, NULL);
  661,529,913 ( 9.05%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/stdlib/strtod.c:strtod (1,000,000x)
```


La taille de la struct est de
```c
struct result {
    char city[100]; // 100 * 1
    int count; // + 4 + 4 de padding
    double sum, min, max; // 3 *8
};

...
struct result results[450];
```

ce qui nous fait `100 + 8 + 24` = **132 bytes** par struct. On a 450 donc `450*132` = **59400 bytes**, ce qui dépasse les 32KB de cache L1 (59400 > 32*1024 = 32768)!!

Inspectons les caches misses, il se trouve que cela n'est pas très impactant, j'en suis un peu étonné. On a 1.5 fois plus que la cache L1, mais cela ne produit que 2.44% de cache misses. Cela n'est pas très stable, cela varie entre 0.4% et 4%...

```sh
> sudo perf stat -e L1-dcache-loads,L1-dcache-load-misses ./build/analyze
Performance counter stats for './build/analyze':

     2,119,149,000      cpu_atom/L1-dcache-loads/                                               (0.41%)
     4,272,031,194      cpu_core/L1-dcache-loads/                                               (99.59%)
   <not supported>      cpu_atom/L1-dcache-load-misses/                                       
       104,350,491      cpu_core/L1-dcache-load-misses/  #    2.44% of all L1-dcache accesses   (99.59%)
```

Il se trouve que les villes de notre dataset ne dépasse pas 26 caractères de long. On pourrait très bien diminuer cette valeur à 30 et on gagnerait un tiers de mémoire, et cela impacte le résultat. Si on passe `char city[30];`, on obtient de manière beaucoup plus stable `0.02%` ou `0.03%`, ce qui est un gain notable. La mesure de différence de temps est trop instable pour être relevée.

A noter en plus que c'est juste le niveau suivant de cache L2 qui prendra la relève...

### Conclusion analyze
Pour améliorer ce code, j'aurai fait dans l'ordre
1. Réduire la taille de plusieurs buffers à quelque chose de plus réaliste
1. Créer une structure hashmap ou tree map qui permet d'accéder en $O(1)$ amorti ou en $O(log(N))$ a une ville pour sa recherche, utilisée dans `getcity`

## Partie 2 - DTMF

### Encoder
Le fichier `txt/verylong.txt` contient 4000 caractères, une suite de dupliqué de l'alphabet supporté. J'utilise les flags suivant `set(CMAKE_C_FLAGS "-g -ggdb -fno-omit-frame-pointer -fno-inline -O2")`.

```sh
> sudo perf record --call-graph dwarf -e branch-misses,branches,cpu-cycles ./build/dtmf_encdec encode txt/verylong.txt output2.wav
```

![perf-three.png](imgs/perf-three.png)

Le résultat est largement contraint par le temps de sauvegarde du fichier via ma fonction `write_wav_file`. `sf_open` et `sf_writef_float` prennent toute la proportion de cette fonctions, à ce stade, il n'y a probablement pas grand chose à optimiser autour puisque c'est 2 fonctions font partie de la librairie libsnd dont on a pas le contrôle direct.
L'encodeur en lui même est à 4.46% de temps d'exécution.

Voici les extraits utiles, les annotations ne concernent que `dtmf_encode`, il n'a pas trouvé audio.c mais cela nous arrange bien pour se concentrer sur le decode surlequel nous avons du contrôle.
```sh
> valgrind --tool=callgrind ./build/dtmf_encdec encode txt/verylong.txt output2.wav
> callgrind_annotate --auto=yes --sort=Ir --show=Ir 
...
dans mix_frequency
52   │ 18,918,892 (16.32%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/math/../sysdeps/ieee754/dbl-64/s_sin.c:__sin_fma (194,040x)
...
dans generate_all_frequencies_buffers
103   │ 21,345,117 (18.41%)  => encoder.c:mix_frequency (97,020x)

dans dtmf_encode
139   │ 22,415,278 (19.33%)  => encoder.c:generate_all_frequencies_buffers (1x)
...
 142   │    16,003 ( 0.01%)      for (int i = 0; i < text_length; i++) {
 143   │         .                   RepeatedBtn *btn = &repeated_btns_for_text[i];
 144   │    12,000 ( 0.01%)          float *src = freqs_buffers[btn->btn_index];
 145   │    50,900 ( 0.04%)          for (uint8_t j = 0; j < btn->repetition; j++) {
 146   │     6,300 ( 0.01%)              if (j > 0) cursor += SHORT_BREAK_SAMPLES_COUNT;// skipping short break silence between repeated tones
 147   │    51,504 ( 0.04%)              memcpy(cursor, src, TONE_SAMPLES_COUNT);
 148   │ 90,862,152 (78.37%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/string/../sysdeps/x86_64/multiarch/memmove-vec-unaligned-erms.S:__memcpy_avx_unaligned_erms (10,300x)
```

On voit que la majorité se passe sur les `memcpy` appelés **10,300 fois** qui visent à copier chaque son (chaque buffer de float pour 8820 samples), avec répétition quand nécessaire. Je me demande si le `__memcpy_avx_unaligned_erms` nous indiquerait pas que nous faisons des accès non alignés, et que nous pourrions gagner à prendre plus de place dans les tableaux derrière `freqs_buffers` pour être aligné ? Ou alors le soucis restera à cause du `cursor` qui sera forcément pas toujours aligné au fur et à mesure qu'on parcourt le fichier...

Mon intuition était que ma fonction `char_to_repeated_btn` était le problème principale, au vu de tous ces branchements et calculs dupliqués pour les mêmes lettres.

Petit extrait pour voir la tête du truc
```c
RepeatedBtn char_to_repeated_btn(char c) {
    // Managing digits first
    if (c >= '0' && c <= '9') {
        char n = c - '0';
        if (n == 0) {
            return (RepeatedBtn) {.btn_index = 10, .repetition = 1};
        } else {
            return (RepeatedBtn) {.btn_index = n - 1, .repetition = 1};
        }
    }

    // And all letters and special chars after
    if (c >= 'a' && c <= 'r') {
        return (RepeatedBtn) {.btn_index = (c - 'a') / 3 + 1,  // + 1 because the cell 0 doesn't have any letters
                              .repetition = (c - 'a') % 3 + 2};// + 2 because we start at 2 repetition at minimum
    } else if (c == 's') {
        return (RepeatedBtn) {.btn_index = 6, .repetition = 5};
...
```
Et bien il se trouve qu'elle ne prend qu'un infime partie du temps de calcul.
```
130   │    67,400 ( 0.06%)  => encoder.c:char_to_repeated_btn (4,000x)
```

Mais j'ai un doute sur cette mesure, peut-être que la fonction est trop courte et que sa mesure est biaisée mais pourtant ce n'est pas avec perf, il n'y a pas ce problème de sampling qui pourrait louper une partie des fonctions appelées.

### Decode float buffer comparaison

