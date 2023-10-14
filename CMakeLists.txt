cmake_minimum_required(VERSION 3.7)

project(Detector)

add_library(detector SHARED detector.c)
find_package(DynamoRIO)
if (NOT DynamoRIO_FOUND)
  message(FATAL_ERROR "DynamoRIO package required to build")
endif(NOT DynamoRIO_FOUND)
configure_DynamoRIO_client(detector)
use_DynamoRIO_extension(detector drmgr)
use_DynamoRIO_extension(detector drsyms)
