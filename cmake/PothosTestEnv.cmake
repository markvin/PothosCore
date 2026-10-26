########################################################################
# Shared environment setup for tests that run against a staged install.
#
# pothos_test_env(PREFIX HOME_DIR BINDIR LIBDIR)
#   - points POTHOS_ROOT at the staged install prefix
#   - isolates user data and config directories inside HOME_DIR
#     so tests never touch the real home directory
#   - makes the staged shared libraries loadable
########################################################################
function(pothos_test_env PREFIX HOME_DIR BINDIR LIBDIR)
    file(REMOVE_RECURSE "${HOME_DIR}")
    file(MAKE_DIRECTORY "${HOME_DIR}/data" "${HOME_DIR}/config")

    set(ENV{POTHOS_ROOT} "${PREFIX}")
    set(ENV{XDG_DATA_HOME} "${HOME_DIR}/data")
    set(ENV{XDG_CONFIG_HOME} "${HOME_DIR}/config")
    set(ENV{APPDATA} "${HOME_DIR}/data")

    if (WIN32)
        file(TO_NATIVE_PATH "${PREFIX}/${BINDIR}" bin_path)
        set(ENV{PATH} "${bin_path};$ENV{PATH}")
    elseif (APPLE)
        set(ENV{DYLD_LIBRARY_PATH} "${PREFIX}/${LIBDIR}:$ENV{DYLD_LIBRARY_PATH}")
    else ()
        set(ENV{LD_LIBRARY_PATH} "${PREFIX}/${LIBDIR}:$ENV{LD_LIBRARY_PATH}")
    endif ()
endfunction()
