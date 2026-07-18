# JingWei
# apps/jw-wpe-browser CMakeLists.txt    2026-07-18
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
pkg_check_modules(WPE_WEBKIT REQUIRED IMPORTED_TARGET
    wpe-webkit-2.0>=2.52.0)

add_executable(jw-wpe-browser
    main.c
)

set_target_properties(jw-wpe-browser PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

target_link_libraries(jw-wpe-browser
    PRIVATE
        JingWei::wpe
        PkgConfig::WPE_WEBKIT
)
