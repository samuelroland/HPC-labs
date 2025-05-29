cmake . -Bbuild && cmake --build build/ -j 8
or return

# clear

set files 1k 10k 100k en.big
set counts 1 2 3 5 10 20
set QUICK_TEST_COUNT 10

# First manual run to generate expected files
if test "$argv[1]" = base
    for count in $counts
        for file in $files
            echo Generating base file: $file for k = $count
            ./build/k-mer-base data/$file.txt $count >expected/$file-$count.txt
        end
    end
    return
end

function run
    set count $argv[2]
    set file "$argv[1]-$count"
    ./build/k-mer data/$file.txt $count >gen/$file.txt
    sort gen/$file.txt >gen/$file.sorted.txt
    if ! test -f expected/$file.sorted.txt
        echo Error: file expected/$file.sorted.txt should exist, run `./tests.fish full`
        return 2
    end
    delta gen/$file.sorted.txt expected/$file.sorted.txt
end

# Regression tests
if test "$argv[1]" = full
    for count in $counts
        for file in $files
            if ! run $file $count
                echo regressions detected !!
                return
            end
        end
    end
else
    for file in $files
        if ! run $file $QUICK_TEST_COUNT
            echo regressions detected !!
            return
        end
    end
end

echo "Regression tests passed !"

hyperfine "taskset -c 3 ./build/k-mer data/100k.txt $count"
