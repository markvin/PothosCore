########################################################################
## Feature registration
########################################################################

########################################################################
# Install cmake helper modules
########################################################################
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/PothosConfigVersion.cmake
    VERSION ${POTHOS_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(FILES
    ${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfig.cmake
    ${PROJECT_SOURCE_DIR}/cmake/Modules/PothosUtil.cmake
    ${PROJECT_BINARY_DIR}/PothosConfigVersion.cmake
    #SIMD
    ${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfigSIMD.cmake

    DESTINATION ${POTHOS_CMAKE_DIRECTORY}
    COMPONENT pothos_devel)
