#!/bin/fish

# Easy printing of colored text
function color
    set_color $argv[1]
    echo $argv[2]
    set_color normal
end

# Build
pushd ../code/
cmake . -Bbuild && cmake --build build/ -j 8
or return
popd

set bin ../code/build/dtmf_encdec

# Run decoding on each file
for file in (ls audio_pack)
    color blue "Decoding $file"
    $bin decode audio_pack/$file
    or return
end
