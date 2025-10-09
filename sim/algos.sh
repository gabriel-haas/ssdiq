set -x

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j

export CAPACITY=128G
export ERASE=8M
export PAGE=4k
export SSDFILL=0.9


GCALGOS="greedy greedy-s2r"

for gc in $GCALGOS; do
	PATTERN=uniform ZONES="" GC=$gc sim/sim > "sim-uni-$gc.csv" & 
	PATTERN=zones ZONES="s0.6 f0.4 s0.4 f0.6" GC=$gc sim/sim > "sim-zone-0.6-$gc.csv" & 
	PATTERN=zones ZONES="s0.7 f0.3 s0.3 f0.7" GC=$gc sim/sim > "sim-zone-0.7-$gc.csv" & 
	PATTERN=zones ZONES="s0.8 f0.2 s0.2 f0.8" GC=$gc sim/sim > "sim-zone-0.8-$gc.csv" & 
	PATTERN=zones ZONES="s0.9 f0.1 s0.1 f0.9" GC=$gc sim/sim > "sim-zone-0.9-$gc.csv" & 

	PATTERN=zones ZONES="s0.95 f0.05 s0.05 f0.95" GC=$gc sim/sim > "sim-zone-0.95-$gc.csv" & 
	PATTERN=zones ZONES="s0.97 f0.03 s0.03 f0.97" GC=$gc sim/sim > "sim-zone-0.97-$gc.csv" & 
	PATTERN=zones ZONES="s0.99 f0.01 s0.01 f0.99" GC=$gc sim/sim > "sim-zone-0.99-$gc.csv" & 

	PATTERN=zones ZONES="s0.5 f0 s0.5 f1" GC=$gc sim/sim > "sim-zone-0.99-$gc.csv" & 
done

