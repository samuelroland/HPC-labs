function measure
    if test -f results.txt
        rm results.txt
    end
    set iterations 40
    for i in (seq 1 $iterations)
        sudo perf stat --json -e power/energy-pkg/ ./build/floatsparty_$argv[1] 1>/dev/null 2>out
        jq -r '.["counter-value"]' <out >>results.txt
    end

    set avg (echo "scale=2; (" (string join + (cat results.txt)) ")" / $iterations | bc)
    echo "Average for $argv[1]: $avg joules"
end
