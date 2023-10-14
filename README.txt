To build:
$ mkdir build && cd ./build
$ cmake -DDynamoRIO_DIR=<DynamoRio Folder>/cmake ./<Project Folder>
$ make

To run:
$ <DynamoRio Folder>/bin64/drrun -c <Project Folder>/build/libdetector.so -- <Program to run and args>
