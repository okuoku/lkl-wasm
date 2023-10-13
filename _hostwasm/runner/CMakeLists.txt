cmake_minimum_required(VERSION 3.12)
project(linrunner C CXX)

include_directories(wasm2c-rt ..)

set(CMAKE_CXX_STANDARD 20)

add_executable(runner
    wasm2c-rt/wasm-rt-impl.c
    ../lin.c
    runner.cpp
)

