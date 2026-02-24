cmake_minimum_required(VERSION 3.10)
project(JingWei LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Check for Linux environment or cross-compilation target
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(fbdev_test playground/fbdev_test.c)

    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(DRM libdrm)
        if(DRM_FOUND)
            add_executable(drm_test playground/drm_test.c)
            target_include_directories(drm_test PRIVATE ${DRM_INCLUDE_DIRS})
            target_link_libraries(drm_test PRIVATE ${DRM_LIBRARIES})
            message(STATUS "Enabled drm_test (libdrm found)")
        else()
            message(STATUS "libdrm not found, skipping drm_test")
        endif()
    else()
        message(STATUS "pkg-config not found, skipping drm_test")
    endif()

else()
    message(STATUS "Skipping Linux-specific tests on non-Linux platform: ${CMAKE_SYSTEM_NAME}")
endif()
