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

########################################################################
# The core as a subproject: the parent and the projects next to the core
# find it with find_package(Pothos) (the in-tree configuration, no install)
########################################################################
if(NOT PROJECT_IS_TOP_LEVEL)
    file(WRITE ${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/PothosConfig.cmake
        "set(POTHOS_IN_TREE_SOURCE_DIR \"${PROJECT_SOURCE_DIR}\")\n"
        "include(\"${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfig.cmake\")\n")
    file(COPY_FILE ${PROJECT_BINARY_DIR}/PothosConfigVersion.cmake
        ${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/PothosConfigVersion.cmake ONLY_IF_DIFFERENT)
endif()

install(FILES
    ${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfig.cmake
    ${PROJECT_SOURCE_DIR}/cmake/Modules/PothosUtil.cmake
    ${PROJECT_BINARY_DIR}/PothosConfigVersion.cmake
    #SIMD
    ${PROJECT_SOURCE_DIR}/cmake/Modules/PothosConfigSIMD.cmake

    DESTINATION ${POTHOS_CMAKE_DIRECTORY}
    COMPONENT pothos_devel)
