cmake . -Bbuild && cmake --build build/ -j 8
or return

# clear

hyperfine -r 3 'taskset -c 3 ./build/k-mer data/100k.txt 10 > gen/100k.txt'

delta gen/100k.txt expected/100k.txt
or regressions detected !!
