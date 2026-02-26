# CMakeLists.txt for multi-process experiment

if(SDL2_FOUND)
    # Server Core
    add_executable(jw_mt_core jw_mt_core.c)
    target_include_directories(jw_mt_core PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(jw_mt_core PRIVATE ${SDL2_LIBRARIES} pthread)

    # Client 1
    add_executable(mt_client mt_client.c)
    # target_link_libraries(mt_client PRIVATE rt)

    # Client 2
    add_executable(mt_client_2 mt_client_2.c)
    # target_link_libraries(mt_client_2 PRIVATE rt)

    if(APPLE)
       target_include_directories(jw_mt_core PRIVATE /opt/homebrew/opt/sdl2/include)
       target_link_directories(jw_mt_core PRIVATE /opt/homebrew/lib)
        set_target_properties(jw_mt_core PROPERTIES 
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "/opt/homebrew/lib;/usr/local/lib"
        )
    endif()

    message(STATUS "Enabled multi_process experiment")
else()
    message(STATUS "SDL2 not found, skipping multi_process experiment")
endif()
