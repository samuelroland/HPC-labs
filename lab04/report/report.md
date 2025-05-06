# HPC Lab 4 - Rapport
Auteur: Samuel Roland

## Baseline
Benchmark du code de départ pour 200 kernels. **1.631s**
```
Benchmark 1: taskset -c 2 ./build/segmentation ../img/sample_640_2.png 200 /tmp/tmp.CklT3lelRc
  Time (mean ± σ):      1.631 s ±  0.017 s    [User: 1.186 s, System: 0.440 s]
  Range (min … max):    1.617 s …  1.655 s    4 runs
 
```


## Optimisations basiques
Après avoir propagé manuellement les valeurs constantes.
```
Benchmark 1: taskset -c 2 ./build/segmentation ../img/sample_640_2.png 200 /tmp/tmp.oYzAAusKn5
  Time (mean ± σ):      1.625 s ±  0.026 s    [User: 1.194 s, System: 0.425 s]
  Range (min … max):    1.603 s …  1.657 s    4 runs
```
En plus des doubles appels à distance, on fait toujours le carré après l'appel de `sqrt` dans `distance` donc on peut simplifier cela.
```c
float dist = distance(src, new_center) * distance(src, new_center);
```


Sortir les valeurs constantes des boucles comme ici `surface` et `sizeOfComponents`
```c
const int surface = image->width * image->height;
const int sizeOfComponents = image->components * sizeof(uint8_t);
    ...
for (int i = 0; i < surface; ++i) {
```

## Allocations inutiles

On a souvent des cas avec des `malloc` inutiles, comme ici on fait une copie des pixels avant de les envoyer à `distance()` qui ne fait pas de modifications donc la copie est complètement inutile.
```c
    // Calculate distances from each pixel to the first center
    uint8_t *dest = malloc(sizeOfComponents);
    memcpy(dest, centers, sizeOfComponents);

    for (int i = 0; i < surface; ++i) {
        uint8_t *src = malloc(sizeOfComponents);
        memcpy(src, image->data + i * image->components, sizeOfComponents);

        distances[i] = distance(src, dest);
```

On peut ainsi simplifier comme ceci est gagner des `free` aussi
```c
    // Calculate distances from each pixel to the first center
    for (int i = 0; i < surface; ++i) {
        distances[i] = distance(image->data + i * image->components, centers);
    }
```

```
Benchmark 1: taskset -c 2 ./build/segmentation ../img/sample_640_2.png 200 /tmp/tmp.SgMsJxhkr7
  Time (mean ± σ):     195.9 ms ±   1.9 ms    [User: 193.2 ms, System: 2.1 ms]
  Range (min … max):   193.6 ms … 197.5 ms    4 runs
```

Après avoir enlevé les copies inutiles à 3 endroits avant des appels à distance, on a gagne énormenent de temps. On passe de **1.625s** à **0.1959 s** !

## Int au lieu de float

En fait, on a pas besoin d'utiliser des float pour la plupart des tailles, ça correspond à des entiers. Les calculs pourraient être accélérés
```c
float distance(uint8_t *p1, uint8_t *p2) {
    float r_diff = p1[0] - p2[0];
    float g_diff = p1[1] - p2[1];
    float b_diff = p1[2] - p2[2];
    return r_diff * r_diff + g_diff * g_diff + b_diff * b_diff;
}
```
Pour s'en convaincre, on peut dumper les float et voir qu'il ne sont jamais très souvent déjà entier ou que leur arrondi ne fera pas grande différence.
```c
total_weight = 248323856.000000 and r = 178121952.000000
total_weight = 228217328.000000 and r = 32316158.000000
total_weight = 211624336.000000 and r = 128449384.000000
total_weight = 210208560.000000 and r = 3426519.750000
total_weight = 199563056.000000 and r = 48471224.000000
```
Je pense qu'on aura pas d'overflow avec des `unsigned int` car `255^2 * 3 = 195075 < INT_MAX = 4294967296`

```
Benchmark 1: taskset -c 2 ./build/segmentation ../img/sample_640_2.png 200 /tmp/tmp.nwC7VQYzOW
  Time (mean ± σ):     132.7 ms ±   0.6 ms    [User: 129.9 ms, System: 2.3 ms]
  Range (min … max):   131.9 ms … 133.4 ms    4 runs
```

Ce qui nous amène à **132.7 ms** !

## SIMD sur distance()

## Résumé des optimisations
| Titre | Temps |
| ------|------- |
| Départ | 1.631s|
| Optimisations basiques | 1.625s|
| Allocations inutiles | 195.9ms |

