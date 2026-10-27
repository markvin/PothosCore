########################################################################
## Feature registration
########################################################################

########################################################################
# build PothosUtil
########################################################################
add_executable(PothosUtil
    PothosUtil.cpp
    PothosUtilSystemInfo.cpp
    PothosUtilModuleInfo.cpp
    PothosUtilSelfTests.cpp
    PothosUtilPluginTree.cpp
    PothosUtilDeviceInfo.cpp
    PothosUtilProxyServer.cpp
    PothosUtilLoadModule.cpp
    PothosUtilDocParse.cpp
    PothosUtilRunTopology.cpp
    PothosUtilListModules.cpp
    PothosUtilProxyEnvironmentInfo.cpp
    PothosUtilSIMDFeatures.cpp
    PothosUtilGenerateSIMDDispatchers.cpp
    PothosUtilListTypeConversions.cpp
)
#internal helpers shared with the library: header-only lib/Util and the
#internal static libraries lib/Platform and lib/Net (built once)
target_include_directories(PothosUtil PRIVATE ${PROJECT_SOURCE_DIR}/lib)
target_link_libraries(PothosUtil PRIVATE
    Pothos
    pothos_platform
    pothos_net
    pothos_app
    pothos_json)
pothos_target_settings(PothosUtil)
install(TARGETS PothosUtil
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT pothos_runtime
)
