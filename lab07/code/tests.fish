cmake . -Bbuild && cmake --build build/ -j 8
or return

# clear

set files 1k 10k 100k ascii-1m
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
                if ! test -f expected/$file-$count.sorted.txt
                    color yellow "skip unavailable file $file"
                    continue
                end
                if ! run $file $count
                    echo regressions detected !!
                    return 2
                end
            end
        end
    else
        for file in $files
            if ! test -f expected/$file-$count.sorted.txt
                color yellow "skip unavailable file $file"
                continue
            end
            if ! run $file $QUICK_TEST_COUNT
                echo regressions detected !!
                return
            end
        end
    end
end

color green "Regression tests passed !"

set RESULT_FILE results.md


# hyperfine -r 4 "taskset -c 3 ./build/k-mer-before-memcmp-opti data/100k.txt $QUICK_TEST_COUNT"

echo """
| k | File | Time before | Time after | CPU usage | Improvement factor |
| - | -- | - | - | - | - |""" >$RESULT_FILE

function append_results
    set count $argv[1]
    set file $argv[2]

    echo -n "|**$count**|`$file`" >>$RESULT_FILE
    set beforetime (jq .results[0].mean < base.$file.$count.out.json)
    set aftertime (jq .results[0].mean < new.$file.$count.out.json)
    printf "|%.4fs" $beforetime >>$RESULT_FILE
    printf "|%.4fs" $aftertime >>$RESULT_FILE
    set factor (echo "scale=2; (" $beforetime / $aftertime ")" | bc)
    printf "|%d%%" (cat percent | grep -oP "\d+") >>$RESULT_FILE
    printf "|%.2fx|\n" $factor >>$RESULT_FILE
    color green (printf "$factor""x: %.4fs -> %.4fs" $beforetime $aftertime)
end

set beforebin k-mer-single-thread
set files 1m ascii-1m 10m.en
set counts 2 5 # 50
for file in $files
    set file $file.txt
    color cyan "File $file"
    for count in $counts
        set runs 10
        if test $count -gt 4
            set runs 3
        end
        if test $file = ascii-1m.txt
            set runs 3
        end
        echo -n "k=$count: "
        # enable this line only the first time
        # hyperfine --max-runs $runs -N "taskset -c 3 ./build/$beforebin data/$file $count" --export-json base.$file.$count.out.json
        # run multithreaded version more times
        hyperfine --max-runs $runs "taskset -c 2-11 /usr/bin/time -v ./build/k-mer data/$file $count |& grep 'Percent of CPU' > percent" --export-json new.$file.$count.out.json
        color blue (cat percent)
        append_results $count $file
    end
end

# set file 100k.txt
# for count in 5 6 7 8 10 20 55 64
#     hyperfine -r 3 --warmup 1 "taskset -c 3 ./build/$beforebin data/$file $count" --export-json out.json
#     hyperfine -r 3 "taskset -c 3 ./build/k-mer data/$file $count" --export-json out2.json
#     append_results $count $file
# end
#
glow $RESULT_FILE
echo
cat $RESULT_FILE
