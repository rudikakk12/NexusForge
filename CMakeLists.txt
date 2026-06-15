cmake_minimum_required(VERSION 4.2)
project(NexusForge)

set(CMAKE_CXX_STANDARD 23)

add_executable(NexusForge main.cpp
        worldgen.cpp
        BLOCKSTATEIDS.h
        ModLoader.h
        main.h
        Core/MacroChunk.hpp
        Core/BlockPlacement.hpp
)
