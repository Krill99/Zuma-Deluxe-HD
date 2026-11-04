#!/bin/bash
set -e
cd /mnt/c/Users/izham/Zuma/Zuma-Deluxe-HD
cp CMakeLists.txt /root/zuma/
cd /root/zuma
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DZUMA_USE_SYSTEM_SDL2=ON -DZUMA_BUILD_AUDIO_TESTS=ON > /dev/null 2>&1
cmake --build build --parallel 8 > build2.log 2>&1
echo "build_exit=$?"
ls build/bin | tr '\n' ' '
echo
cd build/bin
fail=0
for t in music/track*.ogg; do
  ./audio_stream_test "$t" || fail=1
done
echo "stream_exit=$fail"
./audio_cue_test > cue.log 2>&1 || echo "cue failed"
tail -2 cue.log
