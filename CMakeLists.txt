cmake_minimum_required(VERSION 3.31)
project(common)

set(CMAKE_CXX_STANDARD 23)

find_package(aeron)

add_library(common INTERFACE
        include/concurrent/agrona/M2OConcurrentArrayQueue.h
        include/concurrent/agrona/M2ORingBuffer.h
        include/concurrent/agrona/O2ORingBuffer.h
        include/concurrent/agrona/AgronaAtomicBuffer.h
        include/concurrent/agrona/AgronaRingBufferDescriptor.h
)

target_include_directories(common INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(common INTERFACE
        aeron::aeron)


#### Only for CLion or local development... damn that CLion
add_library(common_dummy STATIC
        src/dummy.cpp
        include/concurrent/agrona/M2OConcurrentArrayQueue.h
        include/concurrent/agrona/M2ORingBuffer.h
        include/concurrent/agrona/O2ORingBuffer.h
        include/concurrent/agrona/AgronaAtomicBuffer.h
        include/concurrent/agrona/AgronaRingBufferDescriptor.h)

target_include_directories(common_dummy PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(common_dummy
        aeron::aeron
)
#################################################


install(TARGETS common
        EXPORT commonTargets)

install(EXPORT commonTargets
        FILE commonTargets.cmake
        NAMESPACE common::
        DESTINATION lib/cmake/common)

install(DIRECTORY include/ DESTINATION include)