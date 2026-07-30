cmake_minimum_required(VERSION 3.20)

# Must be set before project(): statically links the MSVC runtime so the
# DLL doesn't depend on a matching Visual C++ Redistributable being present
# on every target VM.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(NppCronHighlighter LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Notepad++ plugins only build for Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(NppCronHighlighter SHARED
    src/dllmain.cpp
    src/PluginDefinition.cpp
    src/CronDetector.cpp
    src/CronDescriber.cpp
    src/CronHighlighter.cpp
    src/CronTooltip.cpp
)

target_include_directories(NppCronHighlighter PRIVATE
    ${CMAKE_SOURCE_DIR}/vendor/npp-sdk
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(NppCronHighlighter PRIVATE UNICODE _UNICODE)

set_target_properties(NppCronHighlighter PROPERTIES
    OUTPUT_NAME "NppCronHighlighter"
    SUFFIX ".dll"
)

# Notepad++ loads plugins matching the host's bitness. Modern Notepad++
# installs are x64, so build with an x64 generator/toolset
# (e.g. `cmake -B build -A x64`) unless you specifically target a 32-bit
# Notepad++ install.
