cmake_minimum_required(VERSION 3.10)
project(JingWei LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Check for Linux environment or cross-compilation target
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(fbdev_test playground/fbdev_test.c)
else()
    message(STATUS "Skipping fbdev_test on non-Linux platform: ${CMAKE_SYSTEM_NAME}")
endif()
