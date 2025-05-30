# Rapport HPC Lab 7 - Parallélisme des tâches - Samuel Roland

[[toc]]

<!-- TODO support toc ? -->

## Première partie — Analyse des k-mers

todo fichiers utilisés !

### Baseline
Le code tel quel nous un résultat assez différent selon le nombre de `k` mais voici sur 100'000 décimales de PI le résultat. Plus `k` est élevé plus le temps est long. Nous sommes à **16.210s**.
```sh
> hyperfine './build/k-mer data/100k.txt 10'
  Time (mean ± σ):     16.210 s ±  1.538 s    [User: 15.869 s, System: 0.246 s]
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
  Time (mean ± σ):     14.314 s ±  0.991 s    [User: 14.234 s, System: 0.020 s]
```

#### Flags de compilation
Un autre "low hanging fruit" est d'activer `-O3` dans les flags GCC. On arrive à **11.859s**.
```sh
  Time (mean ± σ):     11.859 s ±  1.608 s    [User: 11.758 s, System: 0.022 s]
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

Et le résultat du benchmark pour ce cas est légèrement plus difficile à analyser, selon les exécutions il y a plus ou moins 1s de plus ou moins. Mais la moyenne reste quand même en dessous et le range est plus petit (0.367s au lieu de 1.608s). Nous retiendrons le **10.399s** comme valeur "au milieu" des trois résultats.
```
  Time (mean ± σ):     10.000 s ±  0.367 s    [User: 9.942 s, System: 0.024 s]
  Time (mean ± σ):     10.399 s ±  0.322 s    [User: 10.346 s, System: 0.016 s]
  Time (mean ± σ):     11.073 s ±  0.656 s    [User: 11.006 s, System: 0.028 s]
```

Il reste encore une optimisation avec cette approche: Si il nous retourne pas la taille prévue `if (n != k)`, c'est qu'on est arrivé au bout du fichier, ce qui n'arrivera pas si le paramètre `position` est correctement géré à l'extérieur de l'appel. Ainsi, on peut retirer la condition et ajouter une hypothèse en commentaire.

Notre code devient ainsi très court.

```c
void read_kmer(FILE *f, long position, int k, char *kmer) {
    fseek(f, position, SEEK_SET);
    fread(kmer, sizeof(char), k, f);
}
```

Etonnement retirer le branchement, s'avère pire en moyenne et on a nouveau cette large plage de plus ou moins... Je n'arrive pas à comprendre le sens de cette histoire, c'est étrange. J'ai fixé sur le coeur 3 pour limiter les instabilités.
```sh
Time (mean ± σ):     11.157 s ±  1.338 s    [User: 11.100 s, System: 0.016 s]
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

On voit clairement que le syscall `lseek` est appelé à chaque appel de `fseek` puisqu'il y a le même nombre que de caractères.
```sh
> strace ./build/main &| grep lseek | wc -l
100000
```

Conclusion: se déplacer via le curseur de l'OS est probablement une très mauvaise idée comme il serait possible de déplacer un pointeur coté user space pour faire pareil. On pourrait lire tout le fichier une fois avec `fread` mais je vais partir sur `mmap` qui fait un chargement au fur et à mesure de l'usage.

Ainsi après mappage dans le pointeur `content`, on peut changer ce morceau
```c
    for (long i = 0; i <= file_size - k; i++) {
        fseek(file, i, SEEK_SET);
        fread(kmer, sizeof(char), k, file);

        add_kmer(&table, kmer);
    }
```
avec celui-ci, même plus besoin de copier les k caractères, on peut juste les référencer et ajouter la taille `k` en paramètre de `add_kmer`
```c
    for (long i = 0; i <= file_size - k; i++) {
        add_kmer(&table, content + i, k);
    }
```

A ce point, les temps des 2 versions avaient augmentés sur machine (sans explication particulière) j'ai relancé pour les 2 versions.

Avant changement, **13.156s**
```sh
Time (mean ± σ):     13.156 s ±  0.350 s    [User: 13.092 s, System: 0.021 s]
```

Après changement, on passe à **11.492s**
```sh
Time (mean ± σ):     11.492 s ±  0.131 s    [User: 11.445 s, System: 0.007 s]
```

On observe maintenant que les `lseek` ont bien été remplacé par quelques `mremap` bien moins nombreux.
```
read(3, "43247233888452153437272501285897"..., 1696) = 1696
mmap(NULL, 100000, PROT_READ, MAP_PRIVATE, 3, 0) = 0x7f3e00c65000
mmap(NULL, 217088, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f3e00a26000
mremap(0x7f3e00a26000, 217088, 430080, MREMAP_MAYMOVE) = 0x7f3e009bd000
mremap(0x7f3e009bd000, 430080, 856064, MREMAP_MAYMOVE) = 0x7f3e008ec000
...
```

#### Optimisation de add_kmer par division en sous listes et propre memcmp
<!--
```sh
sam@sxp ~/H/y/H/H/l/code (main)> sudo perf record -e cycles --call-graph dwarf ./build/k-mer data/100k.txt 10
> sudo perf annotate --stdio --source --dsos=./build/k-mer
Error:
The perf.data data has no samples!
```
TODO ne marche pas pourquoi ?!
sudo dnf debuginfo-install glibc
sudo dnf debuginfo-install libstdc++
-->


<!-- Todo: expliquer perf annotate marche pas -->
Je vais passer un coup de profiling pour voir où est passé le plus de temps dans `add_kmer`, pour voir dans quelle direction améliorer pour commencer.  Je profile avec callgrind et seulement 3 et pas 10, parce que le temps de génération est trop lent pour `10`.
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

Comme le fichier ici ne contient que des nombres, on peut s'imaginer une réduction d'environ 10 fois le temps total, puisque les listes sont remplis assez équitablement. La preuve avec ce dataset, le nombre d'entrées par premier caractère est très proche (exemple: il y a `10137` k-mers de avec `k=10` qui commencent par `1`, 9999 qui commencent par 0, 9907 qui commencent par 2, etc).

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
Time (mean ± σ):      1.143 s ±  0.011 s    [User: 1.133 s, System: 0.006 s]
```

**Repassons un coup de profiling**
On voit que le code en plus autour de `add_kmer` n'est pas devenu significatif, on passe de 99.8% à 98.3% seulement.
```c
166,969,262 (98.31%)  => src/main.c:add_kmer (99,998x)
```

C'est toujours la partie recherche et comparaison de strings qui prend la plus grande proportion.
```c
35,107,139 (20.67%)          if (memcmp(table->entries[i].kmer, kmer, k) == 0) {
114,031,336 (67.14%)  => /usr/src/debug/glibc-2.40-25.fc41.x86_64/string/../sysdeps/x86_64/multiarch/memcmp-avx2-movbe.S:__memcmp_avx2_movbe (5,015,305x)
```

Je me demande si en évitant de comparer un caractère (le tout premier qui est déjà égal) dans le cas des lettres et nombres (pas du reste par contre), si on peut gagner du temps ou pas. Eh bien, il se trouve que non, on perd +30ms au lieu d'en gagner.

```sh
Time (mean ± σ):      1.179 s ±  0.006 s    [User: 1.170 s, System: 0.005 s]
```

Je ne garde donc pas ce changement.

J'ai essayé ensuite d'implémenter moi-même le `memcmp` pour éviter l'appel de la fonction et on obtient encore un léger gain de **168ms** avec **1.011s**

```sh
Time (mean ± σ):      1.011 s ±  0.008 s    [User: 1.002 s, System: 0.006 s]
```

Cette fois-ci, la stratégie de skipper le premier caractère évoquée plus haut fonctionne et nous donne encore un gain important de +100ms, nous sommes maintenant à **884.0ms**.

```sh
Time (mean ± σ):     834.5 ms ±   3.7 ms    [User: 827.2 ms, System: 5.0 ms]
```

Je me suis rendu compte une heure plus tard que la stratégie en elle-même est incorrecte, car cela considère `Za` et `za` pareil, puisque je mets en minuscule le première caractère pour choisir la `KmerTable` pour `z` et `Z`, je ne peux pas ignorer le premier caractère au final. J'ai rétabli la version précédente.

#### Optimisation des comparaisons de groupe de 8 et 4 caractères
En fait, la majorité du temps étant passée à comparer des strings, il est encore possible d'améliorer la performance en comparant plus d'un caractère à la fois ! Dans la même idée que le pattern du code SIMD, si `k=21`, on peut comparer 2 fois 8 bytes en les considérant comme deux `u_int64_t`, puis les bytes 17 à 21 c'est 4 bytes à considérer en `u_int32_t` puis finalement le dernier byte à gérer tout seul.

J'ai séparé le code en 2 cas, si `k<4`, rien ne sert d'ajouter de l'overhead de branches qui seront fausses dans tous les cas, je garde le code de départ. Pour tous les autres cas, on essaie de comparer à un multiple de 8 et 4 pour faire le plus possible de comparaisons en `u_int64_t` puis gérer le reste.

En théorie, si `k>=8`, on devrait avoir un peu moins de 8 fois d'accéleration, un peu moins parce qu'il y a un certain overhead non négligeabl de mise en place de ces boucles. Si `4 <= k < 8`, on devrait avoir également un facteur 4.

L'implémentation a eu plusieurs bug de logiques, qui ont heureusement pu être attrapée par mon système de tests anti-regression, qui vérifie que la sortie triée est la même qu'au départ sur tous les fichiers (`1k 10k 100k en.big`) et plusieurs `k` (`1 2 3 5 10 20 50 98`).

Etonnemment, on est loin d'avoir les 4x et 8x de gain...Pour `k=4`, on a 3.3x ce qui est le facteur le plus élevé, mais cela ne reste pas pour k=5, 6 et 7. Pour k=8 on est très très loin du facteur 8 (1.91x). On perd très légèrement pour `k=1` et `k=3` mais cela est largement insignifiant par le reste des gains.

| k | File | Time before | Time after | Improvement factor |
| - | -- | - | - | - |
|**1**|`1m.txt`|0.0030s|0.0032s|0.93x|
|**2**|`1m.txt`|0.0160s|0.0152s|1.04x|
|**3**|`1m.txt`|0.0827s|0.0864s|0.95x|
|**4**|`1m.txt`|0.8465s|0.2558s|3.30x|
|**5**|`100k.txt`|0.5429s|0.2583s|2.10x|
|**6**|`100k.txt`|0.9499s|0.4868s|1.95x|
|**7**|`100k.txt`|1.0180s|0.5329s|1.91x|
|**8**|`100k.txt`|1.0192s|0.5322s|1.91x|
|**10**|`100k.txt`|1.0244s|0.5235s|1.95x|
|**20**|`100k.txt`|1.0113s|0.5211s|1.94x|
|**55**|`100k.txt`|1.0239s|0.5256s|1.94x|
|**64**|`100k.txt`|1.0177s|0.5190s|1.96x|

J'ai testé aussi une suggestion de Copilot de forcer l'alignement sur 8 du champ `kmer` pour que les comparaisons sur 8 et 4 bytes puissent aussi être alignés. ->  `char kmer[MAX_KMER] __attribute__((aligned(8)));`, mais cela ne change rien voir empire légèrement le gain.

#### Optimisation de l'usage du cache
Ma supposition pour le problème précédent est qu'on est limité quelque part par la bande passante mémoire et que notre usage du cache est peu efficace. En effet, de par la structure de `KmerEntry`, on a besoin de 104 bytes pour stocker une entrée, qui parfois ne fait que 9 bytes (si `k=4`, car 4 bytes pour le int + 4 pour les caractères + 1 pour le null byte).

```c
#define MAX_KMER 100
typedef struct {
    unsigned count;
    char kmer[MAX_KMER];
} KmerEntry;
```
Cette taille large, très souvent beaucoup plus grande que nécessaire, fait que nos listes `entries` prennent beaucoup plus de place que nécessaire, ce qui demande plus de ligne de caches à charger au total. Si on arrive à compacter l'espace, cela nous fera encore gagner du temps.

Pour valider cette hypothèse, si on mesure les cache-miss, en L1 on est actuellement à 31% et L3 à 0.5%, pour `k=32` (pas d'événement supporté pour L2).
```sh
> sudo taskset -c 3 perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses ./build/k-mer data/100k.txt 32
     1,117,894,566      cpu_core/L1-dcache-loads/                                             
       337,330,960      cpu_core/L1-dcache-load-misses/  #   30.18% of all L1-dcache accesses 
       297,271,724      cpu_core/LLC-loads/                                                   
         1,451,345      cpu_core/LLC-load-misses/        #    0.49% of all LL-cache accesses  
```


Si on teste le minimum nécessaire pour `k=32`, avec `#define MAX_KMER 33` au lieu de `100`, on passe à **12.26%** en L1 et **0.02%** en L3, et cela se traduit aussi par un gain de temps: d'un coup les facteurs augmentent de 1.5-2 fois dès qu'on dépasse `k=4`!
<!-- 
```sh
> sudo taskset -c 3 perf stat -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses ./build/k-mer data/100k.txt 32
     <not counted>      cpu_atom/L1-dcache-loads/                                               (0.00%)
     1,116,339,777      cpu_core/L1-dcache-loads/                                             
   <not supported>      cpu_atom/L1-dcache-load-misses/                                       
       136,914,562      cpu_core/L1-dcache-load-misses/  #   12.26% of all L1-dcache accesses 
     <not counted>      cpu_atom/LLC-loads/                                                     (0.00%)
        69,938,891      cpu_core/LLC-loads/                                                   
     <not counted>      cpu_atom/LLC-load-misses/                                               (0.00%)
            16,894      cpu_core/LLC-load-misses/        #    0.02% of all LL-cache accesses  
```
-->



**Attention: Dans tous les tableaux de cette section "Time before" se base sur le temps de la version avant la section précédente, pour voir si les facteurs augmentent vers les valeurs attendues.**
| k | File | Time before | Time after | Improvement factor |
| - | -- | - | - | - |
|**1**|`1m.txt`|0.0029s|0.0031s|0.94x|
|**2**|`1m.txt`|0.0161s|0.0153s|1.05x|
|**3**|`1m.txt`|0.0816s|0.0801s|1.01x|
|**4**|`1m.txt`|0.8511s|0.2321s|3.66x|
|**5**|`100k.txt`|0.5577s|0.1379s|4.04x|
|**6**|`100k.txt`|0.9547s|0.2424s|3.93x|
|**7**|`100k.txt`|1.0197s|0.2627s|3.88x|
|**8**|`100k.txt`|1.0259s|0.2423s|4.23x|
|**10**|`100k.txt`|1.0290s|0.2441s|4.21x|

Cet essai temporaire n'est pas viable puisque on ne veut pas être limité à 32 caractères pour `k`, mais bien rester à 100. Mais en fait, au lieu de copier constamment ces `k` bytes dans des buffers séparés trop grand, pourquoi ne pas juste stocker un pointeur vers le premier caractère et ne pas gérer le null byte ? Cela est possible comme la taille est toujours pareil (toujours `k`) et il suffit de garder le fichier ouvert jusqu'au bout pour pointer sur son contenu durant les analyses. Cela nous permet de passer de 104 bytes à 12 bytes.

Le gain est très net, les facteurs 4 restent stables et augmentent même vers en facteur 5. Au final, on est passé avec ce changement pour `k=64` de **519ms à 200ms**. Le facteur pour `k=10` est passé de **1.95x à 5.17x** !

| k | File | Time before | Time after | Improvement factor |
| - | -- | - | - | - |
|**1**|`1m.txt`|0.0030s|0.0033s|0.92x|
|**2**|`1m.txt`|0.0164s|0.0163s|1.00x|
|**3**|`1m.txt`|0.0820s|0.0875s|0.93x|
|**4**|`1m.txt`|0.8532s|0.1930s|4.42x|
|**5**|`100k.txt`|0.5515s|0.1180s|4.67x|
|**6**|`100k.txt`|0.9590s|0.1881s|5.09x|
|**7**|`100k.txt`|1.0261s|0.2038s|5.03x|
|**8**|`100k.txt`|1.0371s|0.2016s|5.14x|
|**10**|`100k.txt`|1.0405s|0.2009s|5.17x|
|**20**|`100k.txt`|1.0326s|0.2011s|5.13x|
|**55**|`100k.txt`|1.0408s|0.1998s|**5.20x**|
|**64**|`100k.txt`|1.0348s|0.2052s|5.04x|

Si on pousse encore plus loin les tests avec le fichier `1m.txt`, l'overhead des boucles et branchements rajoutés prend de moins de place et le facteur grandit encore, jusqu'à **9.59x** au maximum.

| k | File | Time before | Time after | Improvement factor |
| - | -- | - | - | - |
|**5**|`1m.txt`|9.9946s|1.9949s|5.01x|
|**6**|`1m.txt`|127.8112s|13.3154s|**9.59x**|
|**10**|`1m.txt`|241.4931s|28.8358s|**8.37x**|
|**20**|`1m.txt`|243.9942s|29.0963s|8.38x|
|**55**|`1m.txt`|241.1902s|29.2314s|8.25x|
|**64**|`1m.txt`|238.9192s|28.8191s|8.29x|

#### Problème de grosse latence sur ASCII random
Tous mes benchmark jusqu'à maintenant ont été réalisés sur des fichiers avec des chiffres, j'ai donc ignoré la possibilité d'avoir des fichiers avec des caractères ASCII autre qu'alphabétique et numéros. J'ai généré un fichier `ascii-1m.txt` (1million de caractères ASCII visibles, entre code le 32 et 126). Sans surprise au vu de la stratégie actuel, le fichier ascii est super long à traiter alors qu'il a la même taille.

```sh
Benchmark 1: taskset -c 3 ./build/k-mer data/1m.txt 2
  Time (mean ± σ):      16.2 ms ±   0.1 ms    [User: 15.2 ms, System: 0.8 ms]
Benchmark 1: taskset -c 3 ./build/k-mer data/ascii-1m.txt 2
  Time (mean ± σ):     566.5 ms ±   1.6 ms    [User: 564.6 ms, System: 0.8 ms]
```

J'ai donc simplifié ma structure de donnée qui était inutilement compliqué pour gagner un poil de mémoire, en la remplaçant par un tableau `KmerTable tables[ASCII_COUNT];`. Cela a également eu le bénéfice de simplifier une partie du code. On revient dans des grandeurs beaucoup plus proches. La différence restante est normale, puisqu'il y a 95 caractères différents au lieu de 10, ce qui fait des listes plus longues.
```sh
Benchmark 1: ./build/k-mer data/1m.txt 2
  Time (mean ± σ):      16.7 ms ±   0.6 ms    [User: 15.7 ms, System: 0.9 ms]
Benchmark 1: ./build/k-mer data/ascii-1m.txt 2
  Time (mean ± σ):      46.9 ms ±   1.5 ms    [User: 45.8 ms, System: 0.8 ms]
```

### Conclusion de la préparation
J'ai travaillé sur de nombreux aspects afin d'améliorer le code existant mono-threadé, la majorité du temps était passée à chercher une entrée existante dans la liste et en comparant les caractères, cette partie a été optimisée un bon morceau. Je suis allé plus loin que demandé dans cette partie parce que c'était vraiment fun de pousser le challenge si loin.

Voici un résumé des améliorations sur le fichier `100k.txt` avec `k=10`.

| Section                                                                 | Temps     |  Facteur relatif (à l'étape précédente) | Facteur absolu |
| ----------------------------------------------------------------------- | --------- | ------------------------------- | ------------------------------ |
| Départ                                                                  | **16.210s**   |  -                               | 1                              |
| Ouverture des fichiers                                                  | 14.314s   |  1.133x                          | 1.133x                          |
| Flags de compilation                                                    | 11.859s   |  1.207x                          | 1.367x                          |
| Refactoring de read_kmer                                                | 11.492s   |  1.032x                          | 1.411x                          |
| Optimisation de add_kmer par division en sous listes et propre memcmp   | 834.5ms   |  **13.77x**                           | 19.43x                          |
| Optimisation des comparaisons de groupe de 8 et 4 caractères            | 523.5ms   |  1.595x                           | 30.97x                          |
| Optimisation de l'usage du cache                                        | 200.9ms   |  2.605x                           | **80.70x**                          |


On voit clairement que la section qui a diminué énormément la taille des listes et donc l'espace de recherche a eu un impact massif. Si on voulait continuer encore, il faudrait passer à une hashmap pour diminuer encore largement le nombre d'éléments comparés. A noter aussi que les performances ne sont pas aussi élevées pour les fichiers avec des caractères ASCII aléatoire, puisque tous les caractères spéciaux sont placés dans une seule liste.

### Benchmark général pour développement du multithreading
J'ai choisi de prendre les fichiers `1m.txt` et `ascii-1m.txt` et k = `2 5 50`. J'ai aussi généré un fichier `10m.en.txt` (10MB de notes prises en anglais sur des outils ou cours).

La répartition des caractères n'est pas équitable pour `10m.en.txt` comme le montre cette extrait de `k=1`. Cela va demander de gérer différement que les 2 fichiers précédents la répartition de la charge entre les threads.

```
X: 1650
Y: 6150
Z: 1250
[: 16450
\: 8850
]: 16450
^: 1450
_: 13850
`: 186950
a: 509200
b: 111350
c: 235200
d: 258550
e: 778500
```


| k | File | Temps mono-threadé |
| - | -- | - |
|**2**|`1m.txt`|0.0162s|
|**5**|`1m.txt`|2.2940s|
|**50**|`1m.txt`|27.9965s|
|**2**|`ascii-1m.txt`|0.0449s|
|**5**|`ascii-1m.txt`|4.1614s|
|**50**|`ascii-1m.txt`|4.2618s|


### Stratégie de parallélisation
L'analyse du fichier est parallélisable, puisque chaque kmer peut être recherché et inséré en parallèle, à condition qu'on fasse attention aux incréments du compteurs une fois l'entrée trouvée et durant les réallocations de listes.
En effet, plusieurs race conditions peuvent arriver:
1. si 2 kmer identiques ont trouvés leur `KmerEntry` en même temps, le compteur pourrait n'avoir qu'un seul incrément au lieu de deux
1. si 2 kmer dans la même sous liste sont trouvées en même temps et insérés en bout de liste sur le dernier élément disponible, selon l'ordonnancement effectué, on pourrait avoir 2 insertions au même endroit (une serait perdue)
1. 1 kmer avec deux entrées successives, parce que la première entrée a été inséré juste après que l'autre chercher ait fini de parcourir la liste
1. durant une réallocation la liste n'est pas utilisable, sinon on court le risque d'utiliser la zone précédente ou de réallouer 2 fois et fuiter une des allocations ou d'avoir les paramètres de capacités et taille pas encore tout à fait mis à jour sur le nouveau bloc alloué...

Le parcours du fichier et des entrées en lecture peuvent se faire en parallèle, ça tombe bien puisque c'est justement la recherche qui prend le plus de temps. Par contre, l'écriture du compteur, insertions ou les réallocs doivent se faire sans que personne ne lise la liste associée. On retrouve ainsi un pattern vu en cours de PCO de lecteurs/rédacteurs, une sorte de mutex amélioré qui permet d'accéder à la liste en lecture à plusieurs pour un thread lecteur ou tout seul pour un thread rédacteur.

TODO: fix les typos avec language tool !!!
L'intérêt d'avoir géré les listes séparemment pour chaque premier caractère possible permet de bloquer seulement une liste à la fois en écriture et bloquer temporairement uniquement une partie des threads lecteurs.

Un élément très intéressant que je n'avais pas remarqué et que `k=1` est toujours très rapide, faire un premier tour avec `k=1` peut tout à fait valoir la peine.
```sh
Benchmark 1: ./build/k-mer ./data/ascii-1m.txt 1
  Time (mean ± σ):       3.7 ms ±   0.4 ms    [User: 3.1 ms, System: 0.4 ms]
```

J'utilise la commande suivante pour benchmarker, j'utilise (à nouveau :)) `time -v` pour récupérer un pourcentage d'usage des CPUs globalement, pour avoir une idée d'à quel point on utilise bien nos 10 coeurs. Je pin les threads sur les coeurs 2 à 11. En théorie, il y a 11 threads qui vont tourner (thread principal + 10 démarrés) mais le thread principal ne fera que d'attendre quand les autres travaillent donc je n'attribue pas plus de coeurs physiques.

```sh
hyperfine --max-runs $runs "taskset -c 2-11 /usr/bin/time -v ./build/k-mer data/$file $count |& grep 'Percent of CPU' > percent"
```

#### Parallélisation avec répartition statique
On voit que le temps n'a fait qu'augmenter pour `1m.txt`, c'est normal, aucun thread n'a fait du travail utile sauf le seul qui avait la plage des numéros. On a plus de temps à cause de l'overhead de création des threads. `ascii-1m.txt` a été géré plus rapidement, entre 2 et 2.5 fois plus rapidement, c'est déjà un gain mais cela me parait bizarre que cela soit si petit.

| k | File | Time before | Time after | CPU usage | Improvement factor |
| - | -- | - | - | - | - |
|**2**|`1m.txt`|0.0162s|0.0183s|146|0.88x|
|**5**|`1m.txt`|2.2940s|2.3165s|100|0.99x|
|**50**|`1m.txt`|27.9965s|28.3732s|99|0.98x|
|**2**|`ascii-1m.txt`|0.0449s|0.0213s|531|2.10x|
|**5**|`ascii-1m.txt`|4.1614s|1.4788s|524|2.81x|
|**50**|`ascii-1m.txt`|4.2618s|1.5908s|603|2.67x|

---
* Une explication des éléments inefficaces dans le code fourni, et des améliorations que vous y avez apportées.
* Une analyse des performances de votre version mono-threadée.
* Une description de la stratégie de parallélisation mise en œuvre : répartition du travail entre threads, traitement des cas limites, zones de chevauchement, etc.
* Une comparaison détaillée entre les performances des versions mono et multithreadée (temps d’exécution, scalabilité, goulots d’étranglement...).

## Deuxième partie — Activité DTMF :
* Une description de la partie de votre code qui a été parallélisée.
* Une explication claire de la stratégie de parallélisation utilisée.
* Une justification des choix effectués, et une évaluation de l’intérêt et de l’efficacité de votre parallélisation.
