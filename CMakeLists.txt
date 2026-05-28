cmake_minimum_required(VERSION 3.13)

project(PrecisionDemotionFramework)

set(CMAKE_CXX_STANDARD 17)

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(include)

add_definitions(${LLVM_DEFINITIONS})

add_executable(tool
    src/main.cpp
    src/PrecisionPropagator.cpp
)

add_executable(verify
    src/Verifier.cpp
    src/Benchmark.cpp
)

target_link_libraries(tool
    clangTooling
    clangBasic
    clangASTMatchers
    clangRewrite
)