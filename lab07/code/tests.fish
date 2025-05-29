cmake . -Bbuild && cmake --build build/ -j 8
or return

# clear
set count 10

./build/k-mer data/100k.txt $count >gen/100k.txt
sort gen/100k.txt >gen/100k.sorted.txt
delta gen/100k.sorted.txt expected/100k.sorted.txt
or begin
    echo regressions detected !!
    return
end
echo "Same result generated !"

hyperfine "taskset -c 3 ./build/k-mer data/100k.txt $count"
