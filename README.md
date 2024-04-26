# Export Control Flow Graph
```
$ GHIDRA_INSTALL_DIR=<Ghidra Folder> python3 <Project Folder>/ghidra_exportcfg.py <Target Program> <Output Filename>
```
Note: Run Ghidra at least once

# Build DynamoRIO Client
```
$ cd <Project Folder>
$ mkdir build && cd ./build
$ cmake -DDynamoRIO_DIR=<DynamoRio Folder>/cmake ..
$ make
```

# Run
```
$ <DynamoRio Folder>/bin64/drrun -c <Project Folder>/build/libdetector.so <CFG filename> -- <Program to run and args>
```

# References
* [C Documentation Guide](https://nus-cs1010.github.io/2021-s1/documentation.html)
* [DynamoRIO Sample Tools](https://dynamorio.org/API_samples.html)
