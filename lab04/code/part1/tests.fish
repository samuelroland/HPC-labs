#!/bin/fish

# Easy printing of colored text, use -d for diminued mode
function color
    set args $argv && if [ "$argv[1]" = -d ]
        set_color -d
        set args $argv[2..]
    end && set_color $args[1] && echo $args[2..] && set_color normal
end


# Run tests

mkdir -p tests/gen
cmake . -Bbuild && cmake --build build/ -j 8 || return
clear

set kernels 4 5 6 7 8 9 10 20 50 100
set kernels 4 100
set bench_kernels 100
for k in $kernels
    set base tests/base/$k.png
    set out tests/gen/$k.png
    ./build/segmentation ../img/sample_640_2.png $k $out >/dev/null
    echo -n "Testing with kernel $k: "
    if odiff $base $out >/dev/null
        color green OK
    else
        echo -n ""
        color red FAIL
        echo stopped...
        return
    end
end

for k in $bench_kernels
    set tmp (mktemp)
    hyperfine -r 4 "taskset -c 2 ./build/segmentation ../img/sample_640_2.png $k $tmp"
end
