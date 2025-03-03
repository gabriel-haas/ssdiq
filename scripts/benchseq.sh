#!/bin/bash
set -x 

export FILENAME=$1
export PREFIX=$2
UNI_BS=${3:-4K}
SEQ_BS=${4:-512K}

export IOENGINE=io_uring 
export FILL=1

cmake -DCMAKE_BUILD_TYPE=Release ..
make -j iob

sanitize_nvme() {
    echo "sanitize"
    sudo nvme sanitize --sanact=2 "$FILENAME"
    sleep 2m
    sudo nvme sanitize-log "$FILENAME"
    sleep 10s
    sudo blkdiscard  -v "$FILENAME"
    sleep 1m
}
sanitize_nvme_and_init() {
	sanitize_nvme
	sudo -E FILENAME=$FILENAME INIT=yes IO_SIZE=0 IO_DEPTH=1 BS=4K THREADS=1 PATTERN=sequential RW=1 iob/iob >> iob-output-$PREFIX.csv
}
run_wa_exp_zipf() {
	ZIPF=$1
	sudo -E  FILENAME=$FILENAME INIT=false IO_SIZE_PERCENT_FILE=$OVERWRITE  IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=zipf$SHUFFLE ZIPF=$ZIPF RW=1  iob/iob >> iob-output-$PREFIX.csv
	sleep 2m
}
run_wa_exp_uniform() {
	sudo -E  FILENAME=$FILENAME INIT=false   IO_SIZE_PERCENT_FILE=$OVERWRITE IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=uniform	RW=1 iob/iob >> iob-output-$PREFIX.csv
	sleep 2m
}
run_wa_exp_zones() {
	ZONES=$1
	sudo -E  FILENAME=$FILENAME INIT=false IO_SIZE_PERCENT_FILE=$OVERWRITE  IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=zones$SHUFFLE ZONES=$ZONES RW=1  iob/iob >> iob-output-$PREFIX.csv
	sleep 2m
}


############################################
# ZNS: varying active zones

OVERWRITE=4
for BS in 512K ; do
for ZNS_SIZE in 8G ; do 
for THR in 1 ; do 
for IOD in 1 ; do 
for ZNS_ACTIVE in 1 2 4 8 16 32 ; do 
	sanitize_nvme_and_init
	sudo -E FILENAME=$FILENAME INIT=false   IO_SIZE_PERCENT_FILE=$OVERWRITE IO_DEPTH="$IOD" BS=$BS THREADS="$THR" PATTERN=zns-noshuffle ZNS_ACTIVE_ZONES="$ZNS_ACTIVE" ZNS_ZONE_SIZE="$ZNS_SIZE" RATE=0 RW=1 iob/iob >> iob-output-$PREFIX.csv 
done
done
done
done
done

############################################
# ZNS: varying zone size

OVERWRITE=4
for BS in 512K ; do
for ZNS_SIZE in 1G 2G 4G 8G 16G ; do 
for THR in 1 ; do 
for IOD in 1 ; do 
for ZNS_ACTIVE in 1 ; do 
	sanitize_nvme_and_init
	sudo -E FILENAME=$FILENAME INIT=false   IO_SIZE_PERCENT_FILE=$OVERWRITE IO_DEPTH="$IOD" BS=$BS THREADS="$THR" PATTERN=zns-noshuffle ZNS_ACTIVE_ZONES="$ZNS_ACTIVE" ZNS_ZONE_SIZE="$ZNS_SIZE" RATE=0 RW=1 iob/iob >> iob-output-$PREFIX.csv 
done
done
done
done
done

