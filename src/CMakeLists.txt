set(JINGWEI_SOURCES
    accelerator/soft_renderer.c
    core/jw_buffer.c
    core/jw_context.c
    core/jw_display.c
    event/jw_event.c
    platform/backend/fbdev/jw_backend_fbdev.c
    platform/backend/sdl/jw_backend_sdl.c
    platform/sys/jw_time.c
    proxy/jw_proxy.c
)

add_library(jingwei STATIC ${JINGWEI_SOURCES})
target_include_directories(jingwei
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

if(SDL2_FOUND)
    target_compile_definitions(jingwei PRIVATE JW_HAVE_SDL2=1)
    target_include_directories(jingwei PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(jingwei PUBLIC ${SDL2_LIBRARIES})

    if(APPLE)
        target_include_directories(jingwei PRIVATE /opt/homebrew/opt/sdl2/include)
        target_link_directories(jingwei PUBLIC /opt/homebrew/lib)
        set_target_properties(jingwei PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "/opt/homebrew/lib;/usr/local/lib"
        )
    endif()
endif()
