cmake_minimum_required(VERSION 3.16)
project(csen79_project2_alert_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-O2 -Wall -Wextra)

add_executable(alert_service
    src/main.cpp
    src/parser.cpp
    src/alert_engine.cpp
)
target_include_directories(alert_service PRIVATE include)

add_executable(benchmark
    benchmarks/generator.cpp
    benchmarks/harness.cpp
)
target_include_directories(benchmark PRIVATE include)
