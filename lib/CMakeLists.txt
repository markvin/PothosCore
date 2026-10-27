########################################################################
## Feature registration
########################################################################
cmake_dependent_option(ENABLE_LIBRARY_LIB "Enable Pothos Library.Lib component" ON "ENABLE_LIBRARY" OFF)
add_feature_info("  Lib" ENABLE_LIBRARY_LIB "Pothos development and runtime libraries")
if (NOT ENABLE_LIBRARY_LIB)
    return()
endif()

########################################################################
# Library sources
########################################################################
add_library(Pothos SHARED
    Init.cpp
    Exception.cpp

    Testing/Testing.cpp

    System/Logger.cpp
    System/LoggerSinks.cpp
    System/Builtin/TestLogger.cpp
    System/HostInfo.cpp
    System/NumaInfo.cpp
    System/Exception.cpp
    System/SIMD.cpp

    Object/Object.cpp
    Object/Hash.cpp
    Object/Compare.cpp
    Object/Convert.cpp
    Object/ToString.cpp
    Object/Serialize.cpp
    Object/Exception.cpp

    Object/Builtin/Compare.cpp
    Object/Builtin/ConvertIntermediate.cpp
    Object/Builtin/ConvertIntegers.cpp
    Object/Builtin/ConvertFloats.cpp
    Object/Builtin/ConvertString.cpp
    Object/Builtin/Hash.cpp
    Object/Builtin/Serialize.cpp
    Object/Builtin/Tests.cpp
    Object/Builtin/TestConvertVectors.cpp

    Callable/Callable.cpp
    Callable/CallInterface.cpp
    Callable/CallRegistry.cpp
    Callable/Exception.cpp
    Callable/Tests.cpp

    Framework/Packet.cpp
    Framework/DType.cpp
    Framework/Label.cpp
    Framework/InputPort.cpp
    Framework/OutputPort.cpp
    Framework/Connectable.cpp
    Framework/Block.cpp
    Framework/Topology.cpp
    Framework/TopologyCommit.cpp
    Framework/TopologySquashFlows.cpp
    Framework/TopologyNetworkFlows.cpp
    Framework/TopologyDomainFlows.cpp
    Framework/TopologyDotMarkup.cpp
    Framework/TopologyDumpJSON.cpp
    Framework/TopologyMakeJSON.cpp
    Framework/TopologyStatsJSON.cpp
    Framework/WorkInfo.cpp
    Framework/WorkerActor.cpp
    Framework/WorkerActorPortAllocation.cpp
    Framework/ThreadPool.cpp
    Framework/ThreadEnvironment.cpp
    Framework/SharedBuffer.cpp
    Framework/ManagedBuffer.cpp
    Framework/BufferPool.cpp
    Framework/BufferChunk.cpp
    Framework/BufferConvert.cpp
    Framework/BufferManager.cpp
    Framework/BufferAccumulator.cpp
    Framework/BlockRegistry.cpp
    Framework/Exception.cpp
    Framework/MemoryMappedBufferContainer.cpp

    ConfLoader/ConfLoader.cpp
    ConfLoader/FileRealPath.cpp
    ConfLoader/BlockDescLoader.cpp
    ConfLoader/JSONTopologyLoader.cpp
    ConfLoader/JITCompilerLoader.cpp

    Framework/Builtin/CircularBufferManager.cpp
    Framework/Builtin/TestBufferChunkSerialization.cpp
    Framework/Builtin/TestToString.cpp
    Framework/Builtin/TestBufferConvert.cpp
    Framework/Builtin/TestDoesBlockExist.cpp
    Framework/Builtin/TestDType.cpp
    Framework/Builtin/TestAutomaticPorts.cpp
    Framework/Builtin/TestSharedBuffer.cpp
    Framework/Builtin/GenericBufferManager.cpp
    Framework/Builtin/TestCircularBufferManager.cpp
    Framework/Builtin/TestGenericBufferManager.cpp
    Framework/Builtin/TestBufferManagerWithCustomAllocation.cpp
    Framework/Builtin/TestWorker.cpp
    Framework/Builtin/TestLabel.cpp
    Framework/Builtin/TestThreadPool.cpp
    Framework/Builtin/TestTopology.cpp
    Framework/Builtin/TestDotMarkup.cpp

    Plugin/Path.cpp
    Plugin/Plugin.cpp
    Plugin/Registry.cpp
    Plugin/Module.cpp
    Plugin/ModuleSafeLoad.cpp
    Plugin/ModulePaths.cpp
    Plugin/Static.cpp
    Plugin/Exception.cpp
    Plugin/Tests.cpp

    Proxy/Proxy.cpp
    Proxy/Handle.cpp
    Proxy/Convert.cpp
    Proxy/Environment.cpp
    Proxy/Exception.cpp
    Proxy/Builtin/ConvertContainers.cpp

    Remote/RemoteProxyDatagram.cpp
    Remote/RemoteProxy.cpp
    Remote/RemoteProxyHandle.cpp
    Remote/Server.cpp
    Remote/ServerHandler.cpp
    Remote/Client.cpp
    Remote/Exception.cpp
    Remote/Builtin/TestRemote.cpp

    Managed/Class.cpp
    Managed/Registry.cpp
    Managed/Exception.cpp

    Managed/Builtin/ManagedHandle.cpp
    Managed/Builtin/ManagedProxy.cpp
    Managed/Builtin/ManagedProxy.hpp
    Managed/Builtin/TestManaged.cpp
    Managed/Builtin/TestManagedOpaque.cpp
    Managed/Builtin/TestManagedWildcard.cpp
    Managed/Builtin/TestManagedInheritance.cpp

    Util/UID.cpp
    Util/RefHolder.cpp
    Util/TypeInfo.cpp
    Util/Compiler.cpp
    Util/Network.cpp
    Util/EvalEnvironment.cpp
    Util/BlockDescription.cpp
    Util/Expr/Value.cpp
    Util/Expr/Lexer.cpp
    Util/Expr/Parser.cpp
    Util/Expr/Ops.cpp
    Util/Expr/Eval.cpp
    Util/Expr/Builtins.cpp
    Util/Expr/Convert.cpp
    Util/PropertyFile.cpp


    Util/Builtin/BlockEval.cpp
    Util/Builtin/DeviceInfoUtils.cpp
    Util/Builtin/DocUtils.cpp
    Util/Builtin/SIMDDispatcherUtils.cpp
    Util/Builtin/TestDocUtils.cpp
    Util/Builtin/TestErrorCodeHandling.cpp
    Util/Builtin/TestEvalExpression.cpp
    Util/Builtin/TestExpr.cpp
    Util/Builtin/TestPropertyFile.cpp
    Util/Builtin/TestRingDeque.cpp
    Util/Builtin/TestStringUtils.cpp
    Util/Builtin/TestTempFile.cpp
    Util/Builtin/TestFileSystem.cpp

    Platform/Builtin/TestEnvironment.cpp
    Platform/Builtin/TestDynamicLibrary.cpp
    Platform/Builtin/TestProcess.cpp
    Net/Builtin/TestNetAddress.cpp
    Net/Builtin/TestNetTcp.cpp
    Net/Builtin/TestNetSyslog.cpp
    App/Builtin/TestOptions.cpp
    Util/Builtin/TestSIMDDispatcherUtils.cpp

    Archive/ArchiveEntry.cpp
    Archive/StreamArchiver.cpp
    Archive/TestBasicTypes.cpp
    Archive/TestCustomType.cpp
    Archive/TestPolymorphicType.cpp
    Archive/Exception.cpp
)
add_library(Pothos::Pothos ALIAS Pothos)

########################################################################
# Internal static libraries shared by libPothos and PothosUtil: the
# library exports no internal symbols, so both link these instead of
# compiling the sources twice. They use the public Pothos API as clients
# (no POTHOS_DLL_EXPORTS) and hidden visibility keeps them out of the
# symbols that libPothos exports.
########################################################################
function(pothos_internal_library name)
    add_library(${name} STATIC ${ARGN})
    set_target_properties(${name} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    target_compile_features(${name} PUBLIC cxx_std_23)
    #the Pothos headers and their usage requirements, without a link cycle
    target_include_directories(${name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        $<TARGET_PROPERTY:Pothos,INTERFACE_INCLUDE_DIRECTORIES>)
    target_compile_definitions(${name} PRIVATE $<TARGET_PROPERTY:Pothos,INTERFACE_COMPILE_DEFINITIONS>)
    target_compile_options(${name} PRIVATE $<TARGET_PROPERTY:Pothos,INTERFACE_COMPILE_OPTIONS>)
endfunction()

#internal platform layer (lib/Platform): environment, dynamic libraries, processes
set(platform_sources Platform/CommandLine.cpp)
if(WIN32)
    set(platform_suffix Windows)
    set(platform_libraries iphlpapi) #GetAdaptersAddresses
else()
    set(platform_suffix Unix)
    set(platform_libraries ${CMAKE_DL_LIBS}) #dlopen
endif()
foreach(name Environment DynamicLibrary Process)
    list(APPEND platform_sources Platform/${name}${platform_suffix}.cpp)
endforeach()
pothos_internal_library(pothos_platform ${platform_sources})
target_link_libraries(pothos_platform PUBLIC ${platform_libraries})

#internal network layer (lib/Net) over standalone Asio
pothos_internal_library(pothos_net
    Net/IoContext.cpp
    Net/Uri.cpp
    Net/Address.cpp
    Net/TcpStream.cpp
    Net/TcpServer.cpp
    Net/Syslog.cpp)
target_link_libraries(pothos_net PUBLIC pothos_asio)

#internal application support for PothosUtil (lib/App): command line, server modes
pothos_internal_library(pothos_app
    App/Options.cpp)

target_link_libraries(Pothos PRIVATE pothos_platform pothos_net pothos_app)
if(MSVC)
    #the internal libraries call the Pothos API through dllimport declarations,
    #inside Pothos.dll the linker resolves them locally: "locally defined
    #symbol imported" is expected there
    target_link_options(Pothos PRIVATE /IGNORE:4217,4286)
endif()

if(WIN32)
    target_sources(Pothos PRIVATE WindowsDelayLoadedSymbols.cpp)
    target_sources(Pothos PRIVATE Framework/SharedBufferWindows.cpp)
    target_sources(Pothos PRIVATE Util/FileLockWindows.cpp)
    target_sources(Pothos PRIVATE Util/Builtin/WindowsGetLogicalProcessorInfo.cpp)
elseif(UNIX)
    target_sources(Pothos PRIVATE Framework/SharedBufferUnix.cpp)
    target_sources(Pothos PRIVATE Util/FileLockUnix.cpp)
endif()

target_link_libraries(Pothos PUBLIC ${Pothos_LINKER_FLAGS})
target_include_directories(Pothos PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Pothos PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_compile_options(Pothos PUBLIC ${Pothos_COMPILE_OPTIONS})
target_compile_definitions(Pothos PUBLIC ${Pothos_COMPILE_DEFINITIONS})
set_target_properties(Pothos PROPERTIES SOVERSION ${POTHOS_ABI_VERSION})
set_target_properties(Pothos PROPERTIES VERSION ${POTHOS_LIBVER})
set_target_properties(Pothos PROPERTIES DEFINE_SYMBOL "POTHOS_DLL_EXPORTS")

#symbols are only exported from the library explicitly
set_property(TARGET Pothos PROPERTY C_VISIBILITY_PRESET hidden)
set_property(TARGET Pothos PROPERTY CXX_VISIBILITY_PRESET hidden)
set_property(TARGET Pothos PROPERTY VISIBILITY_INLINES_HIDDEN ON)

if(MSVC)
    target_compile_options(Pothos PRIVATE /bigobj) #builtin convert.cpp too large in debug mode
    target_compile_options(Pothos PRIVATE /Zc:__cplusplus) #report __cplusplus correctly
endif(MSVC)

########################################################################
# Link feature dependencies
########################################################################
#Poco is a transitional private dependency
target_link_libraries(Pothos PRIVATE
    PocoFoundation PocoUtil
    pothos_json)

#C++23 is required by the public headers
target_compile_features(Pothos PUBLIC cxx_std_23)

########################################################################
# Compiler flags that are generally always a good idea to set
# or sensible to use because of the project's constraints.
########################################################################

if(CMAKE_COMPILER_IS_GNUCXX OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    #these warnings are caused by static warnings used throughout the code
    target_compile_options(Pothos PRIVATE -Wno-unused-local-typedefs)

    #common warnings to help encourage good coding practices
    target_compile_options(Pothos PRIVATE -Wall)
    target_compile_options(Pothos PRIVATE -Wextra)
    target_compile_options(Pothos PRIVATE -Wnon-virtual-dtor)
endif()

if(APPLE)
    #fixes issue with duplicate module registry when using application bundle
    target_link_libraries(Pothos PUBLIC "-flat_namespace")
endif()

if(MSVC)
    #we always want to use multiple cores for compilation
    target_compile_options(Pothos PRIVATE /MP)

    #projects should be cross-platform and standard stl functions should work
    target_compile_definitions(Pothos PUBLIC -DNOMINMAX) #enables std::min and std::max

    #suppress the following warnings which are commonly caused by project headers
    target_compile_options(Pothos PUBLIC /wd4251) #disable 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'
    target_compile_options(Pothos PUBLIC /wd4275) #disable non – DLL-interface classkey 'identifier' used as base for DLL-interface classkey 'identifier'
    target_compile_options(Pothos PUBLIC /wd4503) #'identifier' : decorated name length exceeded, name was truncated
endif(MSVC)

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "FreeBSD")
    target_compile_options(Pothos PUBLIC -stdlib=libc++)
endif()

########################################################################
# Configure sources
########################################################################

#force the following CMAKE_INSTALL_*DIR to be relative for the System/Paths.cpp
file(RELATIVE_PATH CMAKE_INSTALL_LIBDIR     "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_FULL_LIBDIR}")
file(RELATIVE_PATH CMAKE_INSTALL_BINDIR     "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_FULL_BINDIR}")
file(RELATIVE_PATH CMAKE_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
file(RELATIVE_PATH CMAKE_INSTALL_DATADIR    "${CMAKE_INSTALL_PREFIX}" "${CMAKE_INSTALL_FULL_DATADIR}")

#get the library name into Paths.cpp for getPothosRuntimeLibraryPath()
target_compile_definitions(Pothos PRIVATE "-DPOTHOS_LIBRARY_NAME=\"$<TARGET_FILE_NAME:Pothos>\"")

if (WIN32)
    set(MODULE_EXT "dll")
elseif (UNIX)
    set(MODULE_EXT "so")
else()
    message(FATAL_ERROR "not win32 or unix")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/System/Paths.in.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Paths.cpp @ONLY)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/System/Version.in.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp @ONLY)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Plugin/Loader.in.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Loader.cpp @ONLY)

target_sources(Pothos PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/Version.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Paths.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Loader.cpp
)

########################################################################
# NUMA info support
########################################################################
#libnuma is a system library: off by default to keep the build self-contained
option(POTHOS_ENABLE_LIBNUMA "Use the system libnuma for NUMA info on Linux" OFF)
if(POTHOS_ENABLE_LIBNUMA)
    find_library(
        NUMA_LIBRARIES
        NAMES numa
        PATHS /usr/lib /usr/lib64
    )
endif()

if(NUMA_LIBRARIES)
    target_compile_definitions(Pothos PRIVATE -DHAVE_LIBNUMA)
    target_sources(Pothos PRIVATE  System/NumaInfoLinux.cpp)
    target_link_libraries(Pothos PRIVATE ${NUMA_LIBRARIES})
    list(APPEND PC_LIBS_PRIVATE "-lnuma")
elseif(WIN32)
    target_sources(Pothos PRIVATE System/NumaInfoWindows.cpp)
elseif(APPLE)
    target_sources(Pothos PRIVATE System/NumaInfoOSX.cpp)
else()
    target_sources(Pothos PRIVATE System/NumaInfoOther.cpp)
endif()

########################################################################
# Link libatomic
########################################################################
find_library(
    ATOMIC_LIBRARIES
    NAMES atomic atomic.so.1 libatomic.so.1
    PATHS /usr/lib /usr/lib64
)

if (ATOMIC_LIBRARIES)
    target_link_libraries(Pothos PRIVATE ${ATOMIC_LIBRARIES})
    list(APPEND PC_LIBS_PRIVATE "-latomic")
endif()

########################################################################
# Thread config support
########################################################################
find_library(
    RT_LIBRARIES
    NAMES rt
    PATHS /usr/lib /usr/lib64
)

if (RT_LIBRARIES)
    target_link_libraries(Pothos PRIVATE ${RT_LIBRARIES})
    list(APPEND PC_LIBS_PRIVATE "-lrt")
endif()

if(WIN32)
    target_sources(Pothos PRIVATE Framework/ThreadConfigWindows.cpp)
elseif(UNIX)
    target_sources(Pothos PRIVATE Framework/ThreadConfigUnix.cpp)
endif()

########################################################################
# OSX support
########################################################################
if (APPLE)
    #Need to link to CoreFoundation to fix dlopen() issue:
    #http://openradar.appspot.com/7209349
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    target_link_libraries(Pothos PRIVATE ${COREFOUNDATION_LIBRARY})
endif()

########################################################################
# Compiler support wrapper
########################################################################
target_sources(Pothos PRIVATE Util/Builtin/TestCompilerSupport.cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Util/Builtin/GccClangCompilerSupport.in.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/GccClangCompilerSupport.cpp @ONLY)
    target_sources(Pothos PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/GccClangCompilerSupport.cpp)
endif()

if(MSVC)
    get_filename_component(MSVC_INSTALL_PATH ${CMAKE_LINKER} PATH)
    file(TO_NATIVE_PATH ${MSVC_INSTALL_PATH} MSVC_INSTALL_PATH)
    string(REPLACE "\\" "\\\\" MSVC_INSTALL_PATH ${MSVC_INSTALL_PATH})
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Util/Builtin/MsvcCompilerSupport.in.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/MsvcCompilerSupport.cpp @ONLY)
    target_sources(Pothos PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/MsvcCompilerSupport.cpp)
endif(MSVC)

########################################################################
# Generate dynamic dispatch files for SIMD code
########################################################################
target_include_directories(Pothos SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/libsimdpp)

include(${CMAKE_CURRENT_SOURCE_DIR}/../libsimdpp/cmake/SimdppMultiarch.cmake)

set(simd_sources
    SIMD/SIMDConvert.cpp)

simdpp_get_runnable_archs(simdpp_runnable_archs)
simdpp_multiarch(
    GeneratedSIMDFiles
    ${simd_sources}
    ${simdpp_runnable_archs})
target_sources(Pothos PRIVATE ${GeneratedSIMDFiles})

########################################################################
# Install the library
########################################################################
install(TARGETS Pothos
    EXPORT PothosExport
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT pothos_runtime # .so file
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT pothos_devel   # .lib file
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT pothos_runtime # .dll file
)

#export target to Pothos project config as Pothos::Pothos
install(EXPORT PothosExport
    NAMESPACE Pothos::
    FILE PothosTargets.cmake
    DESTINATION ${POTHOS_CMAKE_DIRECTORY}
    COMPONENT pothos_devel)

########################################################################
# Build pkg config file
########################################################################
string(REPLACE ";" " " PC_LIBS_PRIVATE "${PC_LIBS_PRIVATE}")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Pothos.in.pc
    ${CMAKE_CURRENT_BINARY_DIR}/Pothos.pc
@ONLY)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/Pothos.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    COMPONENT pothos_devel
)
