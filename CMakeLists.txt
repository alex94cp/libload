cmake_minimum_required(VERSION 3.0)

project(libload LANGUAGES CXX VERSION 1.0.0)

include(CPack)
include(CTest)
include(GNUInstallDirs)

option(LIBLOAD_ENABLE_ARCH_X86_64 "Enable x86_64 support" ON)

option(LIBLOAD_ENABLE_FORMAT_PE32 "Enable module support for the PE format"  OFF)
option(LIBLOAD_ENABLE_FORMAT_PE64 "Enable module support for the PE+ format" OFF)

configure_file(src/config.hpp.cmake src/config.hpp)

find_package(Boost REQUIRED COMPONENTS iostreams unit_test_framework)

add_library(load src/code_chunk.cpp
                 src/mapped_file.cpp
                 src/load_module.cpp
                 src/module_provider.cpp)

if(WIN32)
	target_sources(load PRIVATE src/platform/windows/current_process.cpp
	                            src/platform/windows/local_process.cpp
	                            src/platform/windows/system_module.cpp)
endif()

add_library(LibLoad::load ALIAS load)
target_compile_features(load PUBLIC cxx_std_17)
target_link_libraries(load PUBLIC ${Boost_IOSTREAMS_LIBRARY})

add_library(loadhlp INTERFACE)
get_filename_component(loadhlp_root_dir ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)

if (${LIBLOAD_ENABLE_ARCH_X86_64})
	target_sources(load PRIVATE src/arch/x64/code_generator.cpp)
endif()

if(${LIBLOAD_ENABLE_FORMAT_PE32} OR ${LIBLOAD_ENABLE_FORMAT_PE64})
	find_package(PEPlus REQUIRED)
	target_sources(load PRIVATE src/pe/load_module.cpp)
	target_link_libraries(load PRIVATE PEPlus::peplus)
endif()

install(TARGETS load EXPORT load_targets
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT load_targets FILE LibLoadConfig.cmake NAMESPACE LibLoad::
                            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)

export(EXPORT  load_targets FILE LibLoadTargets.cmake NAMESPACE LibLoad::)

include(GenerateExportHeader)
generate_export_header(load EXPORT_FILE_NAME include/load/export.hpp)
configure_file(include/load/version.hpp.cmake include/load/version.hpp)

target_include_directories(load PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src
                                PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
                                        $<INSTALL_INTERFACE:include> ${Boost_INCLUDE_DIRS})

target_include_directories(loadhlp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
                                             ${CMAKE_CURRENT_BINARY_DIR}/include)

install(DIRECTORY include/load DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN *.hpp)
install(FILES ${CMAKE_BINARY_DIR}/include/load/version.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/load)

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/LibLoadConfig.cmake LibLoadConfig.cmake
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
write_basic_package_version_file(LibLoadConfigVersion.cmake VERSION ${PROJECT_VERSION}
                                                            COMPATIBILITY AnyNewerVersion)

install(FILES ${CMAKE_BINARY_DIR}/LibLoadTargets.cmake
              ${CMAKE_BINARY_DIR}/LibLoadConfigVersion.cmake
              DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libload)

configure_file(include/load/version.hpp.cmake include/load/version.hpp)
install(DIRECTORY include/load DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN *.hpp)
install(FILES ${CMAKE_BINARY_DIR}/include/load/version.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/load)

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/LibLoadConfig.cmake LibLoadConfig.cmake
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
write_basic_package_version_file(LibLoadConfigVersion.cmake VERSION ${PROJECT_VERSION}
                                                            COMPATIBILITY AnyNewerVersion)

install(FILES ${CMAKE_BINARY_DIR}/LibLoadTargets.cmake
              ${CMAKE_BINARY_DIR}/LibLoadConfigVersion.cmake
              DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/libload)

add_library(sample_module MODULE test/sample_module.cpp)
set_target_properties(sample_module PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON
                                               SUFFIX .llm)

add_executable(test_loadmodule test/test_loadmodule.cpp)
target_include_directories(test_loadmodule PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(test_loadmodule LibLoad::load ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_test(NAME LoadModule COMMAND "$<TARGET_FILE:test_loadmodule>"
                         WORKING_DIRECTORY "$<TARGET_FILE_DIR:sample_module>")

add_executable(test_codegenerator test/test_codegenerator.cpp)
target_include_directories(test_codegenerator PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(test_codegenerator LibLoad::load ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_test(NAME CodeGenerator COMMAND "$<TARGET_FILE:test_codegenerator>")