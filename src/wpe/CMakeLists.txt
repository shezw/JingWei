# JingWei
# src/wpe CMakeLists.txt    2026-07-18
#
#      ______     __  __     ______     ______     __     __
#     /\  ___\   /\ \_\ \   /\  ___\   /\___  \   /\ \  _ \ \
#     \ \___  \  \ \  __ \  \ \  __\   \/_/  /__  \ \ \/ ".\ \
#      \/\_____\  \ \_\ \_\  \ \_____\   /\_____\  \ \__/".~\_\
#       \/_____/   \/_/\/_/   \/_____/   \/_____/   \/_/   \/_/.com
#
# @link    : https://github.com/shezw/JingWei
# @author  : shezw
# @email   : hello@shezw.com

find_package(PkgConfig REQUIRED)

pkg_check_modules(WPE_PLATFORM REQUIRED IMPORTED_TARGET
    wpe-platform-2.0>=2.52.0)
pkg_check_modules(WPE_PLATFORM_HEADLESS REQUIRED IMPORTED_TARGET
    wpe-platform-headless-2.0>=2.52.0)
pkg_check_modules(EGL REQUIRED IMPORTED_TARGET egl)

add_library(jingwei_wpe STATIC
    jw_wpe.c
    jw_wpe_input.c
)
add_library(JingWei::wpe ALIAS jingwei_wpe)

set_target_properties(jingwei_wpe PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(jingwei_wpe
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(jingwei_wpe
    PUBLIC
        JingWei::jingwei
        PkgConfig::WPE_PLATFORM
        PkgConfig::WPE_PLATFORM_HEADLESS
    PRIVATE
        PkgConfig::EGL
)
