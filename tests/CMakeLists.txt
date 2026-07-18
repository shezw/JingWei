# JingWei
# tests CMakeLists.txt    2026-07-18
#
# @link    : https://github.com/shezw/jingwei
# @author  : shezw
# @email   : hello@shezw.com

add_executable(jingwei_core_tests test_core.c)
target_link_libraries(jingwei_core_tests PRIVATE JingWei::jingwei)
set_target_properties(jingwei_core_tests PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

add_executable(jingwei_vnc_input_tests test_vnc_input.c)
target_link_libraries(jingwei_vnc_input_tests PRIVATE JingWei::jingwei)
target_compile_definitions(jingwei_vnc_input_tests PRIVATE
    JINGWEI_TEST_HAVE_LIBVNCSERVER=${JINGWEI_HAVE_LIBVNCSERVER}
)
if(JINGWEI_HAVE_LIBVNCSERVER)
    target_link_libraries(jingwei_vnc_input_tests PRIVATE
        PkgConfig::LIBVNCSERVER)
endif()
set_target_properties(jingwei_vnc_input_tests PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

add_test(NAME jingwei.core.frame_stride_and_damage COMMAND jingwei_core_tests frame)
add_test(NAME jingwei.core.invalid_frames COMMAND jingwei_core_tests invalid)
add_test(NAME jingwei.core.event_fifo COMMAND jingwei_core_tests fifo)
add_test(NAME jingwei.core.event_release_safety COMMAND jingwei_core_tests release)
add_test(NAME jingwei.vnc.input_mapping COMMAND jingwei_vnc_input_tests mapping)
if(JINGWEI_HAVE_LIBVNCSERVER)
    add_test(NAME jingwei.vnc.password_validation
        COMMAND jingwei_vnc_input_tests password)
    add_test(NAME jingwei.vnc.network COMMAND jingwei_vnc_input_tests network)
    set_tests_properties(jingwei.vnc.network PROPERTIES TIMEOUT 10)
endif()

if(JINGWEI_ENABLE_WPE)
    add_executable(jingwei_wpe_input_tests test_wpe_input.c)
    target_link_libraries(jingwei_wpe_input_tests PRIVATE JingWei::wpe)
    set_target_properties(jingwei_wpe_input_tests PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
    )
    add_test(NAME jingwei.wpe.modifier_state COMMAND jingwei_wpe_input_tests)
endif()
