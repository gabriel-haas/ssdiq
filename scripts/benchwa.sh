#!/bin/bash
set -x 

FILENAME=$1
export FILESIZE= # let iob figure it out
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
	sudo -E FILENAME=$FILENAME INIT=yes IO_SIZE_PERCENT_FILE=0.01 IO_DEPTH=1 BS=4K THREADS=1 PATTERN=sequential RW=1 iob/iob >> iob-output-$PREFIX.csv
}
run_wa_exp_zipf() {
	ZIPF=$1
	sudo -E FILENAME=$FILENAME INIT=false IO_SIZE_PERCENT_FILE=$OVERWRITE  IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=zipf$SHUFFLE ZIPF=$ZIPF RW=1  iob/iob >> iob-output-$PREFIX.csv
	sleep 2m
}
run_wa_exp_uniform() {
	sudo -E FILENAME=$FILENAME INIT=false   IO_SIZE_PERCENT_FILE=$OVERWRITE IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=uniform	RW=1 iob/iob >> iob-output-$PREFIX.csv
	sleep 2m
}
run_wa_exp_zones() {
	ZONES=$1
	sudo -E FILENAME=$FILENAME INIT=false IO_SIZE_PERCENT_FILE=$OVERWRITE  IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=zones$SHUFFLE ZONES="$ZONES" RW=1  iob/iob >> iob-output-$PREFIX.csv
	sleep 2m
}

SHUFFLE="-shuffle"

############################################
# uniform
OVERWRITE=5
sanitize_nvme_and_init
run_wa_exp_uniform

############################################
# zones READ ONLY
#sanitize_nvme_and_init
#run_wa_exp_zones "s0.5 f1 s0.5 f0"
OVERWRITE=7
sanitize_nvme_and_init
run_wa_exp_zones "s0.2 f0 s0.8 f1"
sanitize_nvme_and_init
run_wa_exp_zones "s0.4 f0 s0.6 f1"
sanitize_nvme_and_init
run_wa_exp_zones "s0.6 f0 s0.4 f1"
sanitize_nvme_and_init
run_wa_exp_zones "s0.8 f0 s0.2 f1"
OVERWRITE=5
sanitize_nvme_and_init
run_wa_exp_zones "s0.9 f0 s0.1 f1"

############################################
# zipf
OVERWRITE=5
sanitize_nvme_and_init
run_wa_exp_zipf 0.2
sanitize_nvme_and_init
run_wa_exp_zipf 0.4
sanitize_nvme_and_init
run_wa_exp_zipf 0.6
sanitize_nvme_and_init
run_wa_exp_zipf 0.8
sanitize_nvme_and_init
OVERWRITE=6
run_wa_exp_zipf 1

############################################
# zones
OVERWRITE=5
sanitize_nvme_and_init
run_wa_exp_zones "s0.6 f0.4 s0.4 f0.6"
sanitize_nvme_and_init
run_wa_exp_zones "s0.7 f0.3 s0.3 f0.7"
OVERWRITE=6
sanitize_nvme_and_init
run_wa_exp_zones "s0.8 f0.2 s0.2 f0.8"
OVERWRITE=7
sanitize_nvme_and_init
run_wa_exp_zones "s0.9 f0.1 s0.1 f0.9"
OVERWRITE=8
sanitize_nvme_and_init
run_wa_exp_zones "s0.95 f0.05 s0.05 f0.95"

