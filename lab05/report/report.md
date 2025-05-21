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

### Conclusion create-sample
Pour améliorer ce code, j'aurai fait dans l'ordre
1. Inliner à la main ou tester l'effet du mot clé `inline` sur `rand_nd`
1. Utiliser un buffer beaucoup plus grand pour stocker les résultats à inscrire dans le fichier de sortie pour limiter un maximum les syscalls effectués pour write le fichier
1. Utiliser un autre algorithme d'aléatoire

## Partie 2 - DTMF

