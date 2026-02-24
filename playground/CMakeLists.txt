# Check for Linux environment or cross-compilation target
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Configuring Linux-specific tests (fbdev, drm)...")

    # fbdev test (Linux only)
    add_executable(fbdev_test fbdev_test.c)

    # DRM test (Linux only, requires libdrm)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(DRM libdrm)
        if(DRM_FOUND)
            add_executable(drm_test drm_test.c)
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
    message(STATUS "Skipping Linux-specific tests (fbdev, drm) on non-Linux platform: ${CMAKE_SYSTEM_NAME}")
endif()

# --- Cross-Platform Tests (SDL2) ---

# On macOS, prioritize Homebrew paths
if(APPLE)
    set(CMAKE_PREFIX_PATH "/opt/homebrew;/usr/local" ${CMAKE_PREFIX_PATH})
    set(CMAKE_FIND_FRAMEWORK LAST) # Prefer libraries over frameworks to avoid broken framework links
endif()

# Try via pkg-config first (often more reliable for Homebrew/Linux)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 sdl2)
endif()

# If pkg-config didn't find it, try CMake's module
if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET)
endif()

if(SDL2_FOUND)
    add_executable(sdl2_test sdl2_test.c)
    target_include_directories(sdl2_test PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(sdl2_test PRIVATE ${SDL2_LIBRARIES})
    
    # Manually add /opt/homebrew/include/SDL2 as a fallback link directory
    if(APPLE) 
       target_include_directories(sdl2_test PRIVATE /opt/homebrew/opt/sdl2/include)
       target_link_directories(sdl2_test PRIVATE /opt/homebrew/lib)
    endif()

    # Fix for macOS RPATH if referencing dylibs directly
    if(APPLE)
        set_target_properties(sdl2_test PROPERTIES 
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "/opt/homebrew/lib;/usr/local/lib"
        )
    endif()

    message(STATUS "Enabled sdl2_test (SDL2 found)")
else()
    message(STATUS "SDL2 not found, skipping sdl2_test. Please install libsdl2-dev or sdl2.")
endif()
