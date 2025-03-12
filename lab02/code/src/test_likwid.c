#include <likwid-marker.h>
#include <stdlib.h>
#include <stdio.h>

#define BLOCK_SIZE 256

void init(int* ai, double* af, int SIZE){
	for(int i = 0; i < SIZE; ++i){
		ai[i] = rand();
		af[i] = rand();
	}
}

void cache_clean(int *tmp, int CS){
	for(int i = 0; i < CS; ++i){
		tmp[i] = rand();
	}
}

int main(int argc, char** argv){
	int count = 0;
	double count_fp = 0.0;
	//Ici cache_size est initialisé à 4MB car il va nous permettre de remplir les caches 
	// entièrement afin d'être certain que array et array_f soit dans la DRAM est plus en cache.
	size_t cache_size = 4*1024*1024;
	size_t SIZE = atoi(argv[1]);
	int *array = malloc(sizeof(*array)*SIZE);
	double *array_f = malloc(sizeof(*array_f)*SIZE);

	//tmp aura donc un size de 16MB ce qui nous assure de remplir 
	// nos 3 caches (L1 32kB, L2 256kB et L3 8MB)
	int *tmp = malloc(cache_size*sizeof(int));
	
	if(tmp == NULL){
		printf("tmp pointer is null");
		return -1;
	} else if (array == NULL) {
		printf("array pointer is null");
		return -1;
	} else if (array_f == NULL){
		printf("array_f pointer is null");
		return -1;
	}
	
	init(array, array_f, SIZE);
	cache_clean(tmp, cache_size);
	
	LIKWID_MARKER_INIT;
	LIKWID_MARKER_START("int");
	for(int i = 0; i < SIZE; ++i){
		count += array[i] * i;
	}
	LIKWID_MARKER_STOP("int");

	LIKWID_MARKER_START("double");
	for(int i = 0; i < SIZE; ++i){
		count_fp += array_f[i] * (double)i;
	}
	LIKWID_MARKER_STOP("double");
	LIKWID_MARKER_CLOSE;
	
	printf("int READS %ld, double READS %ld\n", sizeof(int)*SIZE, sizeof(double)*SIZE);

	return count + count_fp;
}