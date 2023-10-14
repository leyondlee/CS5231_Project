## To build:
```
$ cd <Project Folder>
$ mkdir build && cd ./build
$ cmake -DDynamoRIO_DIR=<DynamoRio Folder>/cmake ..
$ make
```

## To run:
```
$ <DynamoRio Folder>/bin64/drrun -c <Project Folder>/build/libdetector.so -- <Program to run>
```
