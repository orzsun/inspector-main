cmake_minimum_required(VERSION 3.20)

project(packet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(packet
    src/main.cpp
    src/LogParser.cpp
)

target_include_directories(packet PRIVATE src)

if(MSVC)
    target_compile_options(packet PRIVATE /W4 /permissive-)
else()
    target_compile_options(packet PRIVATE -Wall -Wextra -Wpedantic)
endif()
