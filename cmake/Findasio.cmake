########################################################################
# Find standalone Asio (headers only) when it has no CMake package
# configuration (the system packages of Arch, Debian and Ubuntu).
#
# Sets asio_FOUND, asio_INCLUDE_DIR, asio_VERSION and makes the
# imported target asio::asio.
########################################################################
find_path(asio_INCLUDE_DIR NAMES asio.hpp asio/version.hpp)

if(asio_INCLUDE_DIR AND EXISTS "${asio_INCLUDE_DIR}/asio/version.hpp")
    #ASIO_VERSION has the format XXYYZZ: 103802 is 1.38.2
    file(STRINGS "${asio_INCLUDE_DIR}/asio/version.hpp" asio_version_line
        REGEX "^#define ASIO_VERSION [0-9]+")
    string(REGEX MATCH "[0-9]+" asio_version_number "${asio_version_line}")
    if(asio_version_number)
        math(EXPR asio_version_major "${asio_version_number} / 100000")
        math(EXPR asio_version_minor "${asio_version_number} / 100 % 1000")
        math(EXPR asio_version_patch "${asio_version_number} % 100")
        set(asio_VERSION "${asio_version_major}.${asio_version_minor}.${asio_version_patch}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(asio
    REQUIRED_VARS asio_INCLUDE_DIR
    VERSION_VAR asio_VERSION)
mark_as_advanced(asio_INCLUDE_DIR)

if(asio_FOUND AND NOT TARGET asio::asio)
    add_library(asio::asio INTERFACE IMPORTED)
    set_target_properties(asio::asio PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${asio_INCLUDE_DIR}")
endif()
