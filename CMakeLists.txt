cmake_minimum_required(VERSION 3.13)
project(JingWei VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

option(JINGWEI_BUILD_EXAMPLES "Build JingWei examples" ON)
option(JINGWEI_BUILD_PLAYGROUND "Build JingWei playground programs" ON)
option(JINGWEI_ENABLE_VNC "Build LibVNCServer support when available" ON)
option(JINGWEI_REQUIRE_VNC
    "Fail configuration unless the LibVNCServer backend is available"
    OFF)

if(JINGWEI_REQUIRE_VNC AND NOT JINGWEI_ENABLE_VNC)
    message(FATAL_ERROR
        "JINGWEI_REQUIRE_VNC=ON requires JINGWEI_ENABLE_VNC=ON")
endif()

set(JINGWEI_DEFAULT_ENABLE_SDL_BACKEND ON)
set(JINGWEI_DEFAULT_ENABLE_FBDEV_BACKEND OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(JINGWEI_DEFAULT_ENABLE_SDL_BACKEND OFF)
    set(JINGWEI_DEFAULT_ENABLE_FBDEV_BACKEND ON)
endif()

option(JINGWEI_ENABLE_SDL_BACKEND
    "Build the SDL display backend"
    ${JINGWEI_DEFAULT_ENABLE_SDL_BACKEND})
option(JINGWEI_ENABLE_FBDEV_BACKEND
    "Build the fbdev display backend"
    ${JINGWEI_DEFAULT_ENABLE_FBDEV_BACKEND})

if(APPLE)
    set(CMAKE_PREFIX_PATH "/opt/homebrew;/usr/local" ${CMAKE_PREFIX_PATH})
    set(CMAKE_FIND_FRAMEWORK LAST)
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

if(JINGWEI_ENABLE_SDL_BACKEND)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 sdl2)
    endif()
    if(NOT SDL2_FOUND)
        find_package(SDL2 QUIET)
    endif()
endif()

set(JINGWEI_WPE_DEFAULT OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(JINGWEI_DETECT_WPE_PLATFORM QUIET
        wpe-platform-2.0>=2.52.0)
    pkg_check_modules(JINGWEI_DETECT_WPE_HEADLESS QUIET
        wpe-platform-headless-2.0>=2.52.0)
    pkg_check_modules(JINGWEI_DETECT_WPE_WEBKIT QUIET
        wpe-webkit-2.0>=2.52.0)
    pkg_check_modules(JINGWEI_DETECT_EGL QUIET egl)

    if(JINGWEI_DETECT_WPE_PLATFORM_FOUND
        AND JINGWEI_DETECT_WPE_HEADLESS_FOUND
        AND JINGWEI_DETECT_WPE_WEBKIT_FOUND
        AND JINGWEI_DETECT_EGL_FOUND)
        set(JINGWEI_WPE_DEFAULT ON)
    endif()
endif()
option(JINGWEI_ENABLE_WPE
    "Build the WPE 2.52 JingWei adapter and browser shell"
    ${JINGWEI_WPE_DEFAULT})

if(JINGWEI_ENABLE_WPE AND NOT JINGWEI_WPE_DEFAULT)
    message(FATAL_ERROR
        "JINGWEI_ENABLE_WPE requires wpe-platform-2.0, "
        "wpe-platform-headless-2.0, wpe-webkit-2.0 (>= 2.52), and EGL")
endif()

add_subdirectory(src)

target_sources(jingwei PRIVATE
    src/core/event_queue.c
    src/core/surface.c
    src/backends/vnc/vnc_backend.c
    src/platform/backend/vnc/jw_backend_vnc.c
)
add_library(JingWei::jingwei ALIAS jingwei)

set_target_properties(jingwei PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    OUTPUT_NAME jingwei
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(jingwei
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
)
target_link_libraries(jingwei PUBLIC Threads::Threads)
if(UNIX)
    target_compile_definitions(jingwei PRIVATE _POSIX_C_SOURCE=200809L)
endif()

set(JINGWEI_HAVE_LIBVNCSERVER 0)
if(JINGWEI_ENABLE_VNC)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBVNCSERVER QUIET IMPORTED_TARGET libvncserver)
    endif()

    if(TARGET PkgConfig::LIBVNCSERVER)
        set(JINGWEI_HAVE_LIBVNCSERVER 1)
        target_link_libraries(jingwei PRIVATE PkgConfig::LIBVNCSERVER)
        message(STATUS "JingWei LibVNCServer backend enabled")
    elseif(JINGWEI_REQUIRE_VNC)
        message(FATAL_ERROR
            "JINGWEI_REQUIRE_VNC=ON but LibVNCServer was not found")
    else()
        message(STATUS
            "LibVNCServer not found; JingWei VNC API will report unavailable")
    endif()
endif()
target_compile_definitions(jingwei PRIVATE
    JINGWEI_HAVE_LIBVNCSERVER=${JINGWEI_HAVE_LIBVNCSERVER}
)

if(JINGWEI_ENABLE_WPE)
    message(STATUS "JingWei WPE 2.52 adapter and browser shell enabled")
    add_subdirectory(src/wpe)
    add_subdirectory(apps/jw-wpe-browser)
else()
    message(STATUS
        "WPE 2.52 development packages not found; JingWei WPE integration disabled")
endif()

if(JINGWEI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

if(JINGWEI_BUILD_PLAYGROUND)
    add_subdirectory(playground)
endif()

install(TARGETS jingwei
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(FILES include/jingwei.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/jingwei DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
