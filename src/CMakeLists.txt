set(JINGWEI_SOURCES
    accelerator/soft_renderer.c
    core/jw_buffer.c
    core/jw_context.c
    core/jw_display.c
    event/jw_event.c
    platform/sys/jw_time.c
    proxy/jw_proxy.c
)

if(JINGWEI_ENABLE_FBDEV_BACKEND)
    list(APPEND JINGWEI_SOURCES platform/backend/fbdev/jw_backend_fbdev.c)
else()
    list(APPEND JINGWEI_SOURCES platform/backend/fbdev/jw_backend_fbdev_stub.c)
endif()

if(JINGWEI_ENABLE_SDL_BACKEND)
    list(APPEND JINGWEI_SOURCES platform/backend/sdl/jw_backend_sdl.c)
else()
    list(APPEND JINGWEI_SOURCES platform/backend/sdl/jw_backend_sdl_stub.c)
endif()

add_library(jingwei STATIC ${JINGWEI_SOURCES})
target_include_directories(jingwei
    PUBLIC ${CMAKE_CURRENT_LIST_DIR}/../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

if(JINGWEI_ENABLE_SDL_BACKEND AND SDL2_FOUND)
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
