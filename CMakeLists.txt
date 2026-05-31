cmake_minimum_required(VERSION 3.10)
project(JingWei LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(JINGWEI_BUILD_EXAMPLES "Build JingWei examples" ON)
option(JINGWEI_BUILD_PLAYGROUND "Build JingWei playground programs" ON)

if(APPLE)
    set(CMAKE_PREFIX_PATH "/opt/homebrew;/usr/local" ${CMAKE_PREFIX_PATH})
    set(CMAKE_FIND_FRAMEWORK LAST)
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 sdl2)
endif()
if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
endif()

add_subdirectory(src)
if(JINGWEI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(JINGWEI_BUILD_PLAYGROUND)
    add_subdirectory(playground)
endif()
