cmake_minimum_required(VERSION 3.10)
project(linrunner C CXX)

include_directories(wasm2c-rt ..)

add_executable(runner
    wasm2c-rt/wasm-rt-impl.c
    ../lin.c
    runner.cpp
)

