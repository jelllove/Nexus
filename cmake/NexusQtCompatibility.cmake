function(nexus_fix_legacy_qt_agl)
    if(NOT APPLE OR NOT TARGET WrapOpenGL::WrapOpenGL)
        return()
    endif()

    get_target_property(nexus_qt_gl_links WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
    if(NOT nexus_qt_gl_links)
        return()
    endif()

    # Older Qt exports unused AGL as either a framework flag or a system framework path.
    set(nexus_original_qt_gl_links "${nexus_qt_gl_links}")
    list(FILTER nexus_qt_gl_links EXCLUDE REGEX "(^-framework AGL$|(^|/)AGL\\.framework(/.*)?$)")
    if(NOT nexus_qt_gl_links STREQUAL nexus_original_qt_gl_links)
        set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY INTERFACE_LINK_LIBRARIES "${nexus_qt_gl_links}")
        message(STATUS "Removed Qt's obsolete AGL dependency; OpenGL and other dependencies are unchanged.")
    endif()
endfunction()
