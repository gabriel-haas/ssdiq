# SSD-iq

🚨 **IMPORTANT NOTICE:** 🚨  
**This repository contains supporting materials for a paper currently under submission.  
The contents are subject to change based on revisions and feedback.**  

---

This repository contains supplementary material for the SSD-iq paper, including the IOB benchmarking tool, scripts for running benchmarks, and additionally, the data, simulator, and scripts used to generate the plots in the paper.

## 📁 Repository Contents
- `iob/` - Source code of the IOB benchmarking tool  
- `sim/` - Source code of the GC simulator  
- `scripts/` - Contains the scripts for benchmarking  
- `paper/` - Supporting material for the paper, including data and scripts to generate plots  

## ⚙️ Building

```sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## 🚀 Running IOB

```sh
FILENAME=/blk/k0 FILESIZE=10G IOENGINE=io_uring INIT=disable IO_SIZE=5 IO_DEPTH=128 BS=4K THREADS=4 PATTERN=uniform RW=0 iob/iob
```

## 🛠️ Running the GC Simulator

```sh
CAPACITY=20G ERASE=1M PAGE=4K SSDFILL=0.875 PATTERN=zones ZONES="s0.9 f0.1 s0.1 f0.9" GC=greedy WRITES=10 sim/sim
```

## 📊 Reproducibility: Benchmarks, Scripts & Plots

The `scripts/` folder contains all scripts used to gather the data presented in the paper:
- `benchwa.sh` - Used for all write amplification-related benchmarks (Zipf, Zones, Read-Only)
- `benchseq.sh` - Used for ZNS-like workloads
- `benchlat.sh` - Used for latency under load experiments
- `benchbench.sh` - Gathers all data points for the benchmark summary table

`iob` generates several log files, which can be evaluated using the R scripts provided in the `paper/` folder:
- `paper.R` - Generates all write amplification and throughput-related plots
- `latency.R` - Generates latency plots
- `plotsim.R` - Generates all simulator-based plots

## 📜 Citation & License

- ❌ **No license has been assigned** to this repository at this time.  
- 📄 Once the paper is **published**, we will update this repository with citation details.
