cmake . -Bbuild && cmake --build build/ -j 8
or return

# clear
set count 10

set files 1k 10k 100k en.big

# First manual run to generate expected files
if test "$argv[1]" = base
    for file in $files
        ./build/k-mer-base data/$file.txt $count >expected/$file.txt
    end
    return
end

# Regression tests
for file in $files
    ./build/k-mer data/$file.txt $count >gen/$file.txt
    sort gen/$file.txt >gen/$file.sorted.txt
    if ! delta gen/$file.sorted.txt expected/$file.sorted.txt
        echo regressions detected !!
        return
    end
end

echo "Regression tests passed !"

hyperfine "taskset -c 3 ./build/k-mer data/100k.txt $count"
