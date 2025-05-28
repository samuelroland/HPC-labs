# Rapport HPC Lab 7 - Parallélisme des tâches - Samuel Roland
## Première partie — Analyse des k-mers

### Baseline
Le code tel quel nous un résultat assez différent selon le nombre de `k` mais voici sur 100'000 décimales de PI le résutlat. Plus `k` est élevé plus le temps est long. Nous sommes à **16.210s**.
```sh
> hyperfine './build/k-mer data/100k.txt 10'
Benchmark 1: ./build/k-mer data/100k.txt 10
  Time (mean ± σ):     16.210 s ±  1.538 s    [User: 15.869 s, System: 0.246 s]
  Range (min … max):   14.418 s … 19.183 s    10 runs
```

### Première lecture des éléments inefficaces
#### Ouverture des fichiers
On voit que l'ouverture et fermetures des fichiers n'a pas du tout été optimisé. Dans l'ensemble, on devrait pouvoir ouvrir le fichier, charger tout son contenu en mémoire, le fermer et ne plus avoir besoin de le réouvrir du tout.

Pour calculer la taille, il est ouvert puis refermé.
```c
    FILE *file = fopen(input_file, "r");
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
```

Puis cette boucle va appeler `read_kmer` presque pour chaque caractères et `read_kmer` va également ouvrir et fermer le fichier à chaque fois !! On a donc autant d'ouverture dque de nombre de caractères.
```c
    for (long i = 0; i <= file_size - k; i++) {
        read_kmer(input_file, i, k, kmer);
        add_kmer(&table, kmer);
    }
```

Si on passe le `FILE*` dans la fonction `read_kmer` et on l'ouvre et ferme une seule fois, on passe à **14.314s**, déjà un peu mieux.

```sh
> hyperfine -r 3 './build/k-mer data/100k.txt 10'
Benchmark 1: ./build/k-mer data/100k.txt 10
  Time (mean ± σ):     14.314 s ±  0.991 s    [User: 14.234 s, System: 0.020 s]
  Range (min … max):   13.499 s … 15.417 s    3 runs
```

#### Flags de compilation
Un autre "low hanging fruit" est d'activer `-O3` dans les flags GCC. On arrive à **11.859s**.
```sh
> hyperfine -r 3 './build/k-mer data/100k.txt 10'
Benchmark 1: ./build/k-mer data/100k.txt 10
  Time (mean ± σ):     11.859 s ±  1.608 s    [User: 11.758 s, System: 0.022 s]
  Range (min … max):   10.011 s … 12.942 s    3 runs
```


## TODO
explorer mmpap syscall vs fread, mmap mieux pour accès aléatoire.
mmap, munmap - map or unmap files or devices into memory

---
* Une explication des éléments inefficaces dans le code fourni, et des améliorations que vous y avez apportées.
* Une analyse des performances de votre version mono-threadée.
* Une description de la stratégie de parallélisation mise en œuvre : répartition du travail
entre threads, traitement des cas limites, zones de chevauchement, etc.
* Une comparaison détaillée entre les performances des versions mono et multithreadée
(temps d’exécution, scalabilité, goulots d’étranglement...).

## Deuxième partie — Activité DTMF :
* Une description de la partie de votre code qui a été parallélisée.
* Une explication claire de la stratégie de parallélisation utilisée.
* Une justification des choix effectués, et une évaluation de l’intérêt et de l’efficacité de votre parallélisation.
