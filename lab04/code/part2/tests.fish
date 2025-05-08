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

if [ "$argv[1]" = regen ]
    rm -rf tests/base
    mkdir -p tests/base
    set kernels (seq -10 10 | grep -vP "^0\$")
    for k in $kernels
        set base tests/base/$k.png
        echo generating $k.png
        ./build/weirdimg ../img/sample_640_2.png $k $base >/dev/null
    end
    return
end

set kernels (seq -3 3| grep -vP "^0\$")
# set bench_kernels 4 50 100 1000
for k in $kernels
    set base tests/base/$k.png
    set out tests/gen/$k.png
    ./build/weirdimg ../img/sample_640_2.png $k $out >/dev/null
    echo -n "Testing with kernel $k: "
    if odiff -- $base $out >/dev/null
        color green OK
    else
        echo -n ""
        color red FAIL
        # echo stopped...
        # return
    end
end

echo starting benchmark
set tmp (mktemp)
hyperfine --warmup 1 -r 10 "taskset -c 2 ./build/weirdimg ../img/forest_2k.png 2 $tmp"
