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

## SIMD sur distance()

## Résumé des optimisations
| Titre | Temps |
| ------|------- |
| Départ | 1.631 |
| Optimisations basiques | 1.625 |

