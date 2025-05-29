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

Etonnement retirer le branchement, s'avère pire en moyenne et on a nouveau cette large plage de plus ou moins... Je n'arrive pas à comprendre le sens de cette histoire, c'est étrange. J'ai fixé sur le coeur 3 pour limiter les différences.
```sh
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10 > gen/100k.txt
  Time (mean ± σ):     11.157 s ±  1.338 s    [User: 11.100 s, System: 0.016 s]
  Range (min … max):   10.287 s … 12.698 s    3 runs
```

J'ai aussi inliné la fonction comme elle était trop courte et probablement déjà inliné par `-O2` et plus simple à gérer sur place.

#### fseek+fread à mmap
Je n'étais pas sûr du fonctionnement de fseek, s'il était possible de "déplacer le curseur" sans faire de syscall, j'ai fait un programme séparé pour pouvoir compter les syscalls séparement.
```c
#include <stdio.h>

int main(int argc, char *argv[]) {
    char kmer[100];
    FILE *file = fopen("100k.txt", "r");
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    for (long i = 0; i <= size - 3; i++) {
        fseek(file, i, SEEK_SET);
        fread(kmer, sizeof(char), 3, file);
    }
    fclose(file);
    return 0;
}
```

On voit clairement que le syscall lseek est appelé à chaque appel de `fseek` puisqu'il y a le même nombre que de caractères.
```sh
> strace ./build/main &| grep lseek | wc -l
100000
```

Conclusion: se déplacer via le curseur de l'OS est probablement une très mauvaise idée comme il serait possible de déplacer un pointeur coté user space pour faire pareil. On pourrait lire tout le fichier une fois avec `fread` mais je vais partir sur `mmap` que je ne connais pas encore.

Ainsi après mappage dans le pointeur `content`, on peut changer ce morceau
```c
    for (long i = 0; i <= file_size - k; i++) {
        fseek(file, i, SEEK_SET);
        fread(kmer, sizeof(char), k, file);

        add_kmer(&table, kmer);
    }
```
avec celui-ci, même plus besoin de copier les k caractères, on peut juste les référencer et ajouter la taille `k` à `add_kmer`
```c
    for (long i = 0; i <= file_size - k; i++) {
        add_kmer(&table, content + i, k);
    }
```

A ce point, les temps des 2 versions avaient augmentés sur machine (sans explication particulière) j'ai relancé pour les 2 versions.

Avant changement, **13.156s**
```sh
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10 > gen/100k.txt
  Time (mean ± σ):     13.156 s ±  0.350 s    [User: 13.092 s, System: 0.021 s]
  Range (min … max):   12.804 s … 13.505 s    3 runs
```

Après changement, on passe à **11.492s**
```sh
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10 > gen/100k.txt
  Time (mean ± σ):     11.492 s ±  0.131 s    [User: 11.445 s, System: 0.007 s]
  Range (min … max):   11.383 s … 11.638 s    3 runs
```

On observe maintenant que les `lseek` ont bien été remplacé par quelques `mremap` bien moins nombreux.
```
read(3, "43247233888452153437272501285897"..., 1696) = 1696
mmap(NULL, 100000, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7f3e00c65000
mmap(NULL, 217088, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f3e00a26000
mremap(0x7f3e00a26000, 217088, 430080, MREMAP_MAYMOVE) = 0x7f3e009bd000
mremap(0x7f3e009bd000, 430080, 856064, MREMAP_MAYMOVE) = 0x7f3e008ec000
mremap(0x7f3e008ec000, 856064, 1708032, MREMAP_MAYMOVE) = 0x7f3e0074b000
mremap(0x7f3e0074b000, 1708032, 3411968, MREMAP_MAYMOVE) = 0x7f3e0040a000
mremap(0x7f3e0040a000, 3411968, 6819840, MREMAP_MAYMOVE) = 0x7f3dffd89000
```

#### Optimistaion de add_kmer par profiling
```sh
sam@sxp ~/H/y/H/H/l/code (main)> sudo perf record -e cycles --call-graph dwarf ./build/k-mer data/100k.txt 10
> sudo perf annotate --stdio --source --dsos=./build/k-mer
Error:
The perf.data data has no samples!
```
TODO ne marche pas pourquoi ?!
sudo dnf debuginfo-install glibc
sudo dnf debuginfo-install libstdc++

Je vais passer un coup de profiling pour voir où est passé le plus de temps dans `add_kmer`, pour voir dans quelle direction améliorer pour commencer.
Todo: expliquer perf annotate marche pas

Je profile avec callgrind et seulement 3 et pas 10, parce que le temps de génération est trop lent pour `10`.
```sh
valgrind --tool=callgrind ./build/k-mer data/100k.txt 3
```

J'ai profilé le code du début du labo sans modification et j'ai à nouveau fait l'erreur de ne pas profiler au tout début, `add_kmer` prend **88.5%** du temps, par rapport à `read_kmer` seulement **11.3%*.

```c
    299,998 ( 0.02%)      for (long i = 0; i <= file_size - k; i++) {
    499,990 ( 0.03%)          read_kmer(input_file, i, k, kmer);
194,420,601 (11.30%)  => src/main.c:read_kmer (99,998x)
    699,986 ( 0.04%)          add_kmer(&table, kmer);
1,523,940,811 (88.54%)  => src/main.c:add_kmer (99,998x)
          .               }
```

Avec le code modifié, on voit sans surprise que tout le travail est fait dans cette fonction (99.86% du temps passé dedans)

```c
    299,999 ( 0.02%)      for (long i = 0; i <= file_size - k; i++) {
    799,984 ( 0.05%)          add_kmer(&table, content + i, k);
1,633,147,392 (99.86%)  => src/main.c:add_kmer (99,998x)
          .               }
```

Analysons maintenant qu'est-ce qui prend autant de temps dans `read_kmer`, c'est clairement la recherche de l'entrée à insérer qui prend le plus de temps.
```c
149,261,228 ( 9.13%)      for (int i = 0; i < table->count; i++) {
348,035,874 (21.28%)          if (memcmp(table->entries[i].kmer, kmer, k) == 0) {
1,133,208,368 (69.29%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/string/../sysdeps/x86_64/multiarch/memcmp-avx2-movbe.S:__memcmp_avx2_movbe (49,719,410x)
        673 ( 0.00%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/elf/../sysdeps/x86_64/dl-trampoline.h:_dl_runtime_resolve_xsave (1x)
     98,998 ( 0.01%)              table->entries[i].count++;
          .                       return;
          .                   }
          .               }
```


TODO pertinent ?
J'avais fait l'hypothèse à la première lecture que le `realloc` allait valoir la peine d'être optimisé pour éviter des réallocations, hors cette mesure montre qu'il n'a été appelé que 11fois et prend moins du 1% du temps.
```c
  63   │       1,669 ( 0.00%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/malloc/malloc.c:realloc (11x)
```
TODO: tester avec plus grand > 3 ! maybe ce realloc prend de l'ampleur à ce moment avec beaucoup plus d'entrées.

Benchmark actuel
```
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10 > gen/100k.txt
  Time (mean ± σ):     12.122 s ±  0.349 s    [User: 12.073 s, System: 0.008 s]
  Range (min … max):   11.819 s … 12.503 s    3 runs
```

A cause de cette algorithme en $O(N)$, on doit constamment parcourir une grande partie de la liste avant de trouver l'entrée en question ou arriver tout au bout pour se rendre compte qu'elle n'existe pas. Plus `k` est grand, plus la probabilité est élevée que le mot ne se trouve pas dans la liste et que cette liste soit grande, et que le parcours se fasse en entier. Ce qui explique le temps augmenté plus `k` est grand.

On pourrait tenter d'implémenter ou d'importer une implémenation d'un arbre binaire, qui nous permettrait de faire de rechercher en $O(log(N))$, ou d'une hashmap pour avoir du $O(1)$ amorti. Nous allons essayer de découper en plusieurs morceaux la liste en fonction du premier charactère. J'ai séparé les 10 numéros, des lettres et du reste:

```c
#define LETTERS_COUNT 26
#define NUMBERS_COUNT 10
typedef struct {
    KmerTable letters[LETTERS_COUNT];// 26 KmerTable for words starting with the 26 alphabet letters (case insensitive)
    KmerTable numbers[NUMBERS_COUNT];// 10 KmerTable words starting with for numbers
    KmerTable rest;                  // all other words (starting with special chars)
} KmerTables;
```

Comme le benchmark ici ne traite que des nombres, on peut s'imaginer une réduction d'environ 10 fois le temps total, puisque les listes sont remplis assez équitablement. La preuve avec ce dataset, le nombre d'entrées par premier caractère est très proche (exemple: il y a `10137` k-mers de avec `k=10` qui commencent par `1`)
```
0 9999
1 10137
2 9907
3 10024
4 9968
5 10026
6 10026
7 10025
8 9978
9 9901
```

Le code nécessite plus de boucles d'initialisations et d'affichage des résultats, en plus d'une partie de "routing" vers la bonne sous table

```c
// Pick among one of the 37 available subtable
char firstChar = tolower(content[i]);
KmerTable *subtable = NULL;
if (firstChar >= '0' && firstChar <= '9') {
    subtable = tables.numbers + (firstChar - '0');// get the table of the first digit
} else if (firstChar >= 'a' && firstChar <= 'z') {
    subtable = tables.letters + (firstChar - 'a');// get the table of the first letter
} else {
    subtable = &tables.rest;
}
```
Et c'est effectivement de l'ordre 10x qu'on obtient maintenant **1.143s** !!
```
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10
  Time (mean ± σ):      1.143 s ±  0.011 s    [User: 1.133 s, System: 0.006 s]
  Range (min … max):    1.128 s …  1.159 s    10 runs
```

**Repassons un coup de profiling**
On voit que le code en plus autour de `add_kmer` n'est pas devenu significatif, on passe de 99.8% à 98.3% seulement.
```c
 289   │ 166,969,262 (98.31%)  => src/main.c:add_kmer (99,998x)
```

C'est toujours la partie recherche et comparaison de strings qui prend la plus grande proportion.
```c
 200   │ 35,107,139 (20.67%)          if (memcmp(table->entries[i].kmer, kmer, k) == 0) {
 201   │ 114,031,336 (67.14%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/string/../sysdeps/x86_64/multiarch/memcmp-avx2-movbe.S:__memcmp_avx2_movbe (5,015,305x)
```

Je me demande si en évitant de comparer un caractère (le tout premier qui est déjà égal) dans le cas des lettres et nombres (pas du reste par contre), si on peut gagner du temps ou pas. Eh bien, il se trouve que non, on perd +30ms au lieu d'en gagner.

```sh
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10
  Time (mean ± σ):      1.179 s ±  0.006 s    [User: 1.170 s, System: 0.005 s]
  Range (min … max):    1.171 s …  1.192 s    10 runs
```

Je ne garde donc pas ce changement.

J'ai essayé ensuite d'implémenter moi-même le `memcmp` pour éviter l'appel de la fonction et on obtient encore un léger gain de **168ms** avec **1.011s**

```sh
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10
  Time (mean ± σ):      1.011 s ±  0.008 s    [User: 1.002 s, System: 0.006 s]
  Range (min … max):    1.003 s …  1.029 s    10 runs
```

Cette fois-ci, la stratégie de skippé le premier caractère évoquée plus haut fonctionne et nous donne encore un gain important de +100ms.

```sh
Benchmark 1: taskset -c 3 ./build/k-mer data/100k.txt 10
  Time (mean ± σ):     888.7 ms ±  47.4 ms    [User: 879.2 ms, System: 6.1 ms]
  Range (min … max):   839.8 ms … 992.3 ms    10 runs
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
