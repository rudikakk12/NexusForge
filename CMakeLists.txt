cmake_minimum_required(VERSION 4.2)
project(NexusForge)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mavx2")

add_executable(NexusForge main.cpp
        Core/MacroChunk.hpp
        Core/BlockPlacement.hpp
)
