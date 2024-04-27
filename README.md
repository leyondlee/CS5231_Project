# CS5231_Project
The objectives of this project are to develop a custom DynamoRIO tool for x86-64 ELF binaries to:
* Provide control flow protection using control flow graph and shadow stack
* Detect invalid frees by tracking heap memory allocations

## Prerequisites
* Python 3
* [Ghidra](https://github.com/NationalSecurityAgency/ghidra) (v10.4)
* [DynamoRIO](https://dynamorio.org/) (v9.0.1)

## Export Control Flow Graph
```
$ GHIDRA_INSTALL_DIR=<Ghidra Folder> python3 <Project Folder>/ghidra_exportcfg.py <Target Program> <Output Filename>
```
Note: Run Ghidra at least once

## Build DynamoRIO Client
```
$ cd <Project Folder>
$ mkdir build && cd ./build
$ cmake -DDynamoRIO_DIR=<DynamoRio Folder>/cmake ..
$ make
```

## Run
```
$ <DynamoRio Folder>/bin64/drrun -c <Project Folder>/build/libdetector.so <CFG filename> -- <Program to run and args>
```

## References
* [C Documentation Guide](https://nus-cs1010.github.io/2021-s1/documentation.html)
* [DynamoRIO Sample Tools](https://dynamorio.org/API_samples.html)
