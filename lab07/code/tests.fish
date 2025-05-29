cmake . -Bbuild && cmake --build build/ -j 8
or return

# clear

set files 1k 10k 100k en.big
set counts 1 2 3 5 10 20 50 98
set QUICK_TEST_COUNT 10

# First manual run to generate expected files
if test "$argv[1]" = base
    for count in $counts
        for file in $files
            echo Generating base file: $file for k = $count
            ./build/k-mer-base data/$file.txt $count >expected/$file-$count.txt
            sort expected/$file-$count.txt >expected/$file-$count.sorted.txt
        end
    end
    return
end

# Easy printing of colored text, use -d for diminued mode
function color
    set args $argv && if [ "$argv[1]" = -d ]
        set_color -d
        set args $argv[2..]
    end && set_color $args[1] && echo $args[2..] && set_color normal
end

function run
    set count $argv[2]
    set datafile "$argv[1]"
    set file "$argv[1]-$count"

    echo Running k-mer data/$datafile.txt $count
    ./build/k-mer data/$datafile.txt $count >gen/$file.txt
    sort gen/$file.txt >gen/$file.sorted.txt
    if ! test -f expected/$file.sorted.txt
        color red "Error: file expected/$file.sorted.txt should exist, run `fish tests.fish base`"
        return 2
    end
    delta gen/$file.sorted.txt expected/$file.sorted.txt
end

color blue "Starting regressions tests"
if ! test "$argv[1]" = justbench
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
end

color green "Regression tests passed !"

# hyperfine -r 4 "taskset -c 3 ./build/k-mer-before-memcmp-opti data/100k.txt $QUICK_TEST_COUNT"

for c in 2 3 4
    hyperfine -r 5 "taskset -c 3 ./build/k-mer-before-memcmp-opti data/1m.txt $c"
    hyperfine -r 5 "taskset -c 3 ./build/k-mer data/1m.txt $c"
end

for c in 5 6 7 8 10 20 55 64
    hyperfine -r 4 "taskset -c 3 ./build/k-mer-before-memcmp-opti data/100k.txt $c"
    hyperfine -r 4 "taskset -c 3 ./build/k-mer data/100k.txt $c"
end
