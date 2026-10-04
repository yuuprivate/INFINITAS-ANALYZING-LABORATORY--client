cmake_minimum_required(VERSION 3.20)

project(ANALAB_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(ANALAB_client
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
    src/MusicTableReader.cpp
    src/PlayerProfileReader.cpp
    src/HttpClient.cpp
)

if (MSVC)
    target_compile_options(ANALAB_client PRIVATE 
        /utf-8
        /Gy
        $<$<CONFIG:Release>:/MT>
    )
    target_compile_definitions(ANALAB_client PRIVATE NOMINMAX)
    target_include_directories(ANALAB_client PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/nlohman
    )
    target_link_libraries(ANALAB_client PRIVATE ws2_32 wsock32)

    target_link_options(ANALAB_client PRIVATE
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        $<$<CONFIG:Release>:/VERBOSE:REF>
    )
endif()