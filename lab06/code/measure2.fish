function measure2
    if test -f results.txt
        rm results.txt
    end
    set iterations 40
    for i in (seq 1 $iterations)
        sudo bash perf-markers.sh $argv[1] 1>/dev/null 2>out
        cat out | tail -n 1 >out2
        jq -r '.["counter-value"]' <out2 >>results.txt
    end

    set avg (echo "scale=6; (" (string join + (cat results.txt)) ")" / $iterations | bc)
    echo "Average for $argv[1]: $avg joules"
end
