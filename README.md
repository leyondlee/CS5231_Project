# Build
```
$ cd <Project Folder>
$ mkdir build && cd ./build
$ cmake -DDynamoRIO_DIR=<DynamoRio Folder>/cmake ..
$ make
```

# Run
```
$ <DynamoRio Folder>/bin64/drrun -c <Project Folder>/build/libdetector.so -- <Program to run>
```

# References
* [C Documentation Guide](https://nus-cs1010.github.io/2021-s1/documentation.html)
* [DynamoRIO Sample Tools](https://dynamorio.org/API_samples.html)
