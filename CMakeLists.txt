CMAKE_MINIMUM_REQUIRED(VERSION 3.9)
PROJECT(timesvr)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_FLAGS "-O0")


find_package(gflags COMPONENTS nothreads_static)
find_package(glog COMPONENTS nothreads_static)

LINK_DIRECTORIES(/usr/local/lib)
LINK_DIRECTORIES(/usr/lib64)

ADD_EXECUTABLE(timesvr
        TimeSchema.pb.cc
        TimeSchema.pb.h
        server/TimeServer.cc
        server/ServerCore.cc
        server/ServerCore.h
        server/TimeServer.h)

ADD_EXECUTABLE(timeclt
        TimeSchema.pb.cc
        TimeSchema.pb.h
        client/TimeClient.cc
        )

target_include_directories (timesvr PUBLIC ${CMAKE_SOURCE_DIR})
target_include_directories (timeclt PUBLIC ${CMAKE_SOURCE_DIR})

TARGET_LINK_LIBRARIES(timesvr protobuf event gflags glog)
TARGET_LINK_LIBRARIES(timeclt protobuf)
