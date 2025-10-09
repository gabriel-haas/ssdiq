#!/bin/bash
set -x

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j sim

CAPACITY=32G
ERASE=2M
PAGE=4k
SSDFILL=0.9
WRITES=40

PATTERN=zones
ZONES="s0.9 f0.1 s0.1 f0.9"

counter=0

GC=greedy
./sim/sim --capacity=$CAPACITY --erase=$ERASE --page=$PAGE --ssdfill=$SSDFILL --gc=$GC --pattern=$PATTERN --zones="$ZONES" --writes=$WRITES --switch-dist > output_$counter.txt 2>&1 & 
((counter++))

GC=mdc
for MDC_BATCH in 16 32 64 128; do
    ./sim/sim --capacity=$CAPACITY --erase=$ERASE --page=$PAGE --ssdfill=$SSDFILL --gc=$GC --pattern=$PATTERN --zones="$ZONES" --writes=$WRITES --switch-dist --mdc-batch=$MDC_BATCH > output_$counter.txt 2>&1 &
    ((counter++))
done

GC=2a
for WF in 4 8 16 32; do
    for TT in 1 2 4 8; do
        ./sim/sim --capacity=$CAPACITY --erase=$ERASE --page=$PAGE --ssdfill=$SSDFILL --gc=$GC --pattern=$PATTERN --zones="$ZONES" --writes=$WRITES --switch-dist --writeheads=$WF --timestamps=$TT > output_$counter.txt 2>&1 &
        ((counter++))
    done
done

GC=opt
./sim/sim --capacity=$CAPACITY --erase=$ERASE --page=$PAGE --ssdfill=$SSDFILL --gc=$GC --pattern=$PATTERN --zones="$ZONES" --writes=$WRITES --switch-dist > output_$counter.txt 2>&1 &
((counter++))

# Wait for all background jobs to finish
wait
