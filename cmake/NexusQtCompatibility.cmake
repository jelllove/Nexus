function(nexus_fix_legacy_qt_agl)
    if(NOT APPLE OR NOT TARGET WrapOpenGL::WrapOpenGL)
        return()
    endif()

    get_target_property(nexus_qt_gl_links WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(NOT nexus_qt_gl_links OR NOT "-framework AGL" IN_LIST nexus_qt_gl_links)
        return()
    endif()

    # Older Qt exports this unused framework fallback, removed from the macOS 26 SDK.
    list(REMOVE_ITEM nexus_qt_gl_links "-framework AGL")
    set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY INTERFACE_LINK_LIBRARIES "${nexus_qt_gl_links}")
    message(STATUS "Removed Qt's obsolete AGL fallback; OpenGL and other dependencies are unchanged.")
endfunction()
