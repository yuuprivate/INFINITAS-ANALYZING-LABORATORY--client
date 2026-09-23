cmake_minimum_required(VERSION 3.20)

project(project_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(project_client
    src/main.cpp
    src/Logger.cpp
    src/ProcessFinder.cpp
    src/Process.cpp
    src/MemoryReader.cpp
    src/OffsetManager.cpp
    src/VersionDetector.cpp
    src/VersionResolver.cpp
    src/PatternScanner.cpp
    src/OffsetSearcher.cpp
    src/Module.cpp
    src/JudgeData.cpp
    src/ScoreMapReader.cpp
    src/Tracker.cpp
    src/PlayResult.cpp
    src/PlayData.cpp
    src/CurrentSong.cpp
    src/MusicTableReader.cpp
)

if (MSVC)
    target_compile_options(project_client PRIVATE /utf-8)
    target_compile_definitions(project_client PRIVATE NOMINMAX)
endif()