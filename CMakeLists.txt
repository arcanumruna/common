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
        include/flow/ScopeExit.h
        include/text/StringBuilder.h
        include/numeric/NumberUtils.h
        include/network/MessageBatchView.h
        include/network/RateLimiter.h
        include/network/KeyedRateLimiter.h
        include/network/FrameSyncBackoffIdleStrategy.h
        include/network/FrameSyncIdleStrategy.h
        include/configuration/Configuration.h
        include/network/Network.h
        include/network/SnapshotStore.h
        include/network/FastRNG.h
        include/network/IdGenerator.h
        include/time/Time.h
        include/file/Files.h
        include/filter/ProfanityFilter.h
        include/diagnostics/JemallocCtl.h
        include/diagnostics/DiagnosticsService.h
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
        include/concurrent/agrona/AgronaRingBufferDescriptor.h
        include/flow/ScopeExit.h
        include/text/StringBuilder.h
        include/numeric/NumberUtils.h
        include/network/MessageBatchView.h
        include/network/RateLimiter.h
        include/network/KeyedRateLimiter.h
        include/network/FrameSyncBackoffIdleStrategy.h
        include/network/FrameSyncIdleStrategy.h
        include/configuration/Configuration.h
        include/network/Network.h
        include/network/SnapshotStore.h
        include/network/FastRNG.h
        include/network/IdGenerator.h
        include/time/Time.h
        include/file/Files.h
        include/filter/ProfanityFilter.h
        include/diagnostics/JemallocCtl.h
        include/diagnostics/DiagnosticsService.h)

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
install(DIRECTORY data/ DESTINATION data)



#### Testing
option(BUILD_TESTING "Build tests" OFF)

if(BUILD_TESTING)
    enable_testing()

    find_package(GTest REQUIRED)
    find_package(nlohmann_json REQUIRED)

    add_executable(common_tests
            tests/network/RateLimiterTest.cpp
            tests/network/KeyedRateLimiterTest.cpp
            tests/network/FrameSyncIdleStrategyTest.cpp
            tests/network/MessageBatchViewTest.cpp
            tests/network/RunningAvgTest.cpp
            tests/network/FrameSyncBackoffIdleStrategyTest.cpp
            tests/network/SnapshotStoreTest.cpp
            tests/network/IdGeneratorTest.cpp
            tests/network/FastRNGTest.cpp
            tests/network/NetworkTest.cpp
            tests/collection/ChunkedListTest.cpp
            tests/configuration/ConfigurationTest.cpp
            tests/file/FilesTest.cpp
            tests/flow/ScopeExitTest.cpp
            tests/numeric/NumberUtilsTest.cpp
            tests/text/StringBuilderTest.cpp
            tests/time/TimeTest.cpp
            tests/filter/ProfanityFilterTest.cpp
            tests/diagnostics/DiagnosticsServiceTest.cpp
            tests/crypto/SipHashTest.cpp

            include/concurrent/agrona/M2OConcurrentArrayQueue.h
            include/concurrent/agrona/M2ORingBuffer.h
            include/concurrent/agrona/O2ORingBuffer.h
            include/concurrent/agrona/AgronaAtomicBuffer.h
            include/concurrent/agrona/AgronaRingBufferDescriptor.h
            include/flow/ScopeExit.h
            include/text/StringBuilder.h
            include/numeric/NumberUtils.h
            include/network/MessageBatchView.h
            include/network/RateLimiter.h
            include/network/KeyedRateLimiter.h
            include/network/FrameSyncBackoffIdleStrategy.h
            include/network/FrameSyncIdleStrategy.h
            include/configuration/Configuration.h
            include/network/Network.h
            include/network/SnapshotStore.h
            include/network/FastRNG.h
            include/network/IdGenerator.h
            include/time/Time.h
            include/file/Files.h
            include/filter/ProfanityFilter.h
            include/diagnostics/JemallocCtl.h
            include/diagnostics/DiagnosticsService.h
    )

    target_include_directories(common_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(common_tests PUBLIC
            GTest::gtest_main
            nlohmann_json::nlohmann_json
            aeron::aeron
    )

    include(GoogleTest)
    gtest_discover_tests(common_tests)
endif()

#### Benchmarks
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

if(BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)

    add_executable(common_benchmark
            benchmark/BenchmarkMain.cpp
            benchmark/ProfanityFilterBenchmark.cpp
            include/filter/ProfanityFilter.h
    )

    target_include_directories(common_benchmark PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
    )

    target_link_libraries(common_benchmark PRIVATE
            benchmark::benchmark
            aeron::aeron
    )

    target_compile_definitions(common_benchmark PRIVATE
            PROFANITY_WORD_LIST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/data/profanity_words.txt"
    )
endif()
############################################