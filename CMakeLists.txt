cmake_minimum_required(VERSION 3.7)

project(Detector)

add_compile_options(-Wall)

add_library(detector SHARED src/detector.cpp src/heapnode.cpp src/threadcontext.cpp src/callnode.cpp src/cfgnode.cpp src/cfgsymboledge.cpp src/symbolinfo.cpp)
find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)
configure_DynamoRIO_client(detector)
use_DynamoRIO_extension(detector drmgr)
use_DynamoRIO_extension(detector drsyms)
use_DynamoRIO_extension(detector drwrap)
