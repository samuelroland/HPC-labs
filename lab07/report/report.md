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

#### Refactoring de read_kmer

La fonction `read_kmer` a pour but uniquement de lire `k` bytes et les stocker dans un buffer.

Sur l'implémentation actuelle on trouve plusieurs problèmes. Déjà le `kmer[k] = '\0';` pourrait se faire une seule fois en dehors de la boucle qui appele `read_kmer`, puisque notre `k` ne change pas sa longueur de la chaine extraite ne change pas. Ensuite, appeler `fgetc` pour chaque charactère n'est pas le plus efficace, c'est bufferisé mais on fera mieux d'utiliser `fread` pour lire directement le bon nombre de bytes.
```c
void read_kmer(FILE *f, long position, int k, char *kmer) {
    fseek(f, position, SEEK_SET);
    for (int i = 0; i < k; i++) {
        int c = fgetc(f);
        if (c == EOF) {
            fprintf(stderr, "Error: Reached end of file before reading k-mer.\n");
            fclose(f);
            exit(1);
        }
        kmer[i] = (char) c;
    }
    kmer[k] = '\0';
}
```

Ainsi après avoir déplacé `kmer[k] = '\0';` juste après la définition de `kmer`, cette instruction est lancée une seule fois, et après avoir utilisé `fread`, on obtient ce code

```c
void read_kmer(FILE *f, long position, int k, char *kmer) {
    fseek(f, position, SEEK_SET);
    int n = fread(kmer, sizeof(char), k, f);
    if (n != k) {
        fprintf(stderr, "Error: Reached end of file before reading k-mer.\n");
        fprintf(stderr, "position = %li, kmer = %s, n = %d\n", position, kmer, n);
        fclose(f);
        exit(1);
    }
}
```

Et le résutlat du benchmark pour ce cas est légèrement plus difficile à analyser, selon les exécutions il y a plus ou moins 1s de plus ou moins. Mais la moyenne reste quand même en dessous et le range est plus petit (0.367s au lieu de 1.608s). Nous retiendrons le **10.399s** comme valeur "au milieu" des trois résultats.
```
Benchmark 1: ./build/k-mer data/100k.txt 10 > gen/100k.txt
  Time (mean ± σ):     10.000 s ±  0.367 s    [User: 9.942 s, System: 0.024 s]
  Range (min … max):    9.577 s … 10.231 s    3 runs

Benchmark 1: ./build/k-mer data/100k.txt 10 > gen/100k.txt
  Time (mean ± σ):     10.399 s ±  0.322 s    [User: 10.346 s, System: 0.016 s]
  Range (min … max):   10.036 s … 10.652 s    3 runs

Benchmark 1: ./build/k-mer data/100k.txt 10 > gen/100k.txt
  Time (mean ± σ):     11.073 s ±  0.656 s    [User: 11.006 s, System: 0.028 s]
  Range (min … max):   10.350 s … 11.629 s    3 runs
```

Il reste encore une optimisation avec cette approche: Si il nous retourne pas la taille prévue `if (n != k)`, c'est qu'on est arrivé au bout du fichier, ce qui n'arrivera pas si le paramètre `position` est correctement géré à l'extérieur de l'appel. Ainsi, on peut retirer la condition et ajouter une hypothèse en commentaire.

Notre code devient ainsi très court.

```c
// Read k bytes at index "position" in file f
// The hypothesis is that position <= file_size - k,
// no error will be generated if less bytes are available after position.
void read_kmer(FILE *f, long position, int k, char *kmer) {
    fseek(f, position, SEEK_SET);
    fread(kmer, sizeof(char), k, f);
}
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
