# SSD-iq

🚨 **IMPORTANT NOTICE:** 🚨  
**This repository contains supporting materials for a paper currently under submission.  
The contents are subject to change based on revisions and feedback.**  
🚧 **Do not cite or reference this repository as a final source.** 🚧  

---

This repository contains supplementary material for the SSD-iq paper, including the iob benchmarking tool, scripts for running benchmarks, and additionally, the data, simulator, and scripts to generate the plots in the paper.

## 📁 Repository Contents
- `iob/` - Source code of the iob benchmarking tool  
- `sim/` - Source code of the GC simulator  
- `scripts/` - Contains the scripts for the benchmark  
- `paper/` - Supporting material for the paper, including data and scripts to generate the plots  

## ⚙️ Building

```sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## 🚀 Running iob

```sh
FILENAME=/blk/k0 FILESIZE=10G IOENGINE=io_uring INIT=disable IO_SIZE=5 IO_DEPTH=128 BS=4K THREADS=4 PATTERN=uniform RW=0 iob/iob
```

## 🛠️ Running the GC simulator

```sh
CAPACITY=20G ERASE=1M PAGE=4k SSDFILL=0.875 PATTERN=zones ZONES="s0.9 f0.1 s0.1 f0.9" GC=greedy WRITES=10 sim/sim
```

## 📜 Citation & License

- ❌ **No license has been assigned** to this repository at this time. Usage rights will be determined once the paper is accepted.  
- 📄 Once the paper is **published**, we will update this repository with citation details.  
