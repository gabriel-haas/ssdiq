#!/bin/bash
set -x 

export FILESIZE= # let iob figure it out

FILENAME=$1
PREFIX_PARAM=$2
UNI_BS=${3:-4K}
SEQ_BS=${4:-512K}
LAT_RATE=${5:-100000}

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
    sleep $NORMAL_SLEEP
}
sanitize_nvme_and_init() {
	sanitize_nvme
	sudo -E FILENAME=$FILENAME INIT=disable IO_SIZE_PERCENT_FILE=1 IO_DEPTH=1   BS=$SEQ_BS THREADS=1 PATTERN=sequential RW=1 iob/iob >> iob-output-$PREFIX.csv
	sleep $NORMAL_SLEEP
}
run_wa_exp_zipf() {
	ZIPF=$1
	sudo -E FILENAME=$FILENAME INIT=false IO_SIZE_PERCENT_FILE=$OVERWRITE  IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=zipf$SHUFFLE ZIPF=$ZIPF RW=1  iob/iob >> iob-output-$PREFIX.csv
	sleep $NORMAL_SLEEP
}
run_wa_exp_uniform() {
	sudo -E FILENAME=$FILENAME INIT=false   IO_SIZE_PERCENT_FILE=$OVERWRITE IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=uniform	RW=1 iob/iob >> iob-output-$PREFIX.csv
	sleep $NORMAL_SLEEP
}
run_wa_exp_zones() {
	ZONES=$1
	sudo -E FILENAME=$FILENAME INIT=false IO_SIZE_PERCENT_FILE=$OVERWRITE  IO_DEPTH=128 BS=$UNI_BS THREADS=4 PATTERN=zones$SHUFFLE ZONES="$ZONES" RW=1  iob/iob >> iob-output-$PREFIX.csv
	sleep $NORMAL_SLEEP
}
run_read_exp() {
	POSTFIX=$1
	export PREFIX="$PREFIXBASE-read-seq-$POSTFIX"
	sudo -E FILENAME=$FILENAME INIT=false RUNTIME=$BW_RUNTIME IO_DEPTH=32 BS=$SEQ_BS THREADS=10 PATTERN=uniform RW=0 iob/iob >> iob-output-$PREFIX.csv
	export PREFIX="$PREFIXBASE-read-lat-$POSTFIX"
	sudo -E FILENAME=$FILENAME INIT=false RUNTIME=$PROBE_RUNTIME IO_DEPTH=1 BS=$UNI_BS THREADS=1 PATTERN=uniform RW=0 iob/iob >> iob-output-$PREFIX.csv
	export PREFIX="$PREFIXBASE-read-rand-$POSTFIX"
	sudo -E FILENAME=$FILENAME INIT=false RUNTIME=$BW_RUNTIME IO_DEPTH=128 BS=$UNI_BS THREADS=10 PATTERN=uniform RW=0 iob/iob >> iob-output-$PREFIX.csv
}
run_write_exp() {
	POSTFIX=$1
	export PREFIX="$PREFIXBASE-write-seq-$POSTFIX"
	sudo -E FILENAME=$FILENAME INIT=false RUNTIME=$PROBE_RUNTIME IO_DEPTH=32 BS=$SEQ_BS THREADS=10 PATTERN=uniform RW=1 iob/iob >> iob-output-$PREFIX.csv
	sleep $NORMAL_SLEEP
	export PREFIX="$PREFIXBASE-write-lat-$POSTFIX"
	sudo -E FILENAME=$FILENAME INIT=false RUNTIME=$PROBE_RUNTIME IO_DEPTH=1 BS=$UNI_BS THREADS=1 PATTERN=uniform RW=1 iob/iob >> iob-output-$PREFIX.csv
	sleep $NORMAL_SLEEP
}
run_lat_exp() {
	RATE=$1
	export PREFIX="$PREFIXBASE-load-lat"
	sudo -E FILENAME=$FILENAME INIT=false IO_DEPTH=64 BS=$UNI_BS THREADS=8 PATTERN=$PATTERN$SHUFFLE RW=0.99 RATE=$RATE EXPRATE=$EXPRATE RUNTIME=1000 LATENCYTHREAD=1 iob/iob >> iob-output-$PREFIX.csv
	sleep $NORMAL_SLEEP
}

SHUFFLE="-shuffle"
EXPRATE=1

PROBE_RUNTIME=150
BW_RUNTIME=300
NORMAL_SLEEP=5m
OVERWRITE_UNI=3
OVERWRITE_SKEWED=4

for i in {1..3}; do
PREFIXBASE="$PREFIX_PARAM-$i"
# init
export PREFIX="$PREFIXBASE-init"
sanitize_nvme_and_init
export PREFIX="$PREFIXBASE"
# read 1
run_read_exp 1
# write 1
run_write_exp 1
# workload options:
# 1
    case $i in
        1)
			  OVERWRITE=$OVERWRITE_UNI
			  export PREFIX="$PREFIXBASE-wa-uniform"
			  run_wa_exp_uniform
			  ;;
		  2)
			  # 2
			  OVERWRITE=$OVERWRITE_SKEWED
			  export PREFIX="$PREFIXBASE-wa-zones"
			  run_wa_exp_zones "s0.8 f0.2 s0.2 f0.8"
			  ;;
		  3)
			  # 3
			  OVERWRITE=$OVERWRITE_SKEWED
			  export PREFIX="$PREFIXBASE-wa-zipf"
			  run_wa_exp_zipf 0.8
	  esac
export PREFIX="$PREFIXBASE"
# post exp
# read 2
run_read_exp 2
# write 2
run_write_exp 2
# latency
PATTERN=uniform
run_lat_exp $LAT_RATE

done


