<div align="center">
  <picture>
    <source media="(prefers-color-scheme: light)" srcset="logo/logo.svg">
    <source media="(prefers-color-scheme: dark)" srcset="logo/logo-dark.svg">
    <img alt="SSD-iq logo" src="logo/logo.svg" height="80">
  </picture>
</div>
<br>

## SSD-iq

This repository contains supplementary material for the SSD-iq paper, including the `iob` benchmarking tool and SSD simulator.

📄 **Paper:** [SSD-iq: Uncovering the Hidden Side of SSD Performance](https://www.vldb.org/pvldb/vol18/p4295-haas.pdf)

## Repository Contents
- `iob/` - Source code of the IOB benchmarking tool  
- `sim/` - Source code of the GC simulator  
- `scripts/` - Contains the scripts for benchmarking  
- `paper/` - Supporting material for the paper, including data and scripts to generate plots  

## Building

```sh
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Running IOB

```sh
iob/iob --filename=/blk/w0 --filesize=10G --ioengine=io_uring --init=disable --io_size=10G --iodepth=128 --bs=4K --threads=4 --pattern=uniform --rw=0
```

## Running the GC Simulator

```sh
sim/sim --capacity=20G --erase=1M --page=4k --ssdfill=0.875 --pattern=zones --zones="s0.9 f0.1 s0.1 f0.9" --gc=greedy --writes=10
```

## Reproducibility: Benchmarks, Scripts & Plots

The `scripts/` folder contains all scripts used to gather the data presented in the paper:
- `benchwa.sh` - Used for all write amplification-related benchmarks (Zipf, Zones, Read-Only)
- `benchseq.sh` - Used for ZNS-like workloads
- `benchlat.sh` - Used for latency under load experiments
- `benchbench.sh` - Gathers all data points for the benchmark summary table

`iob` generates several log files, which can be evaluated using the R scripts provided in the `paper/` folder:
- `paper.R` - Generates all write amplification and throughput-related plots
- `latency.R` - Generates latency plots
- `plotsim.R` - Generates all simulator-based plots

## Citation

📄 **Paper:** [SSD-iq: Uncovering the Hidden Side of SSD Performance](https://www.vldb.org/pvldb/vol18/p4295-haas.pdf)

```
@article{DBLP:journals/pvldb/HaasLBL25,
  author       = {Gabriel Haas and
                  Bohyun Lee and
                  Philippe Bonnet and
                  Viktor Leis},
  title        = {SSD-iq: Uncovering the Hidden Side of {SSD} Performance},
  journal      = {Proc. {VLDB} Endow.},
  volume       = {18},
  number       = {11},
  pages        = {4295--4308},
  year         = {2025}
}
```

## License
This project is licensed under the [MIT License](LICENSE).
