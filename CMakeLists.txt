cmake_minimum_required(VERSION 3.0)

project(libload LANGUAGES CXX VERSION 1.0.0)

include(CPack)
include(CTest)
include(GNUInstallDirs)

option(LIBLOAD_ENABLE_PE64_SUPPORT "Enable module support for the PE+ format" ON)
option(LIBLOAD_ENABLE_PE32_SUPPORT "Enable module support for the PE format"  ON)

configure_file(src/config.hpp.cmake src/config.hpp)

find_package(Boost REQUIRED COMPONENTS iostreams unit_test_framework)

add_library(load src/memory.cpp src/mapped_file.cpp src/memory_module.cpp)

if(WIN32)
	target_sources(load PRIVATE src/windows/memory.cpp
	                            src/windows/module.cpp)
endif()

add_library(LibLoad::load ALIAS load)
target_compile_features(load PUBLIC cxx_std_17)
target_link_libraries(load PUBLIC ${Boost_IOSTREAMS_LIBRARY})

if(${LIBLOAD_ENABLE_PE32_SUPPORT} OR ${LIBLOAD_ENABLE_PE64_SUPPORT})
	find_package(PEPlus REQUIRED)
	target_link_libraries(load PRIVATE PEPlus::peplus)
	target_sources(load PRIVATE src/pe/image.cpp src/pe/module.cpp)
endif()

install(TARGETS load EXPORT load_targets
        RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT load_targets FILE LibLoadConfig.cmake NAMESPACE LibLoad::
                            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)

export(EXPORT load_targets  FILE LibLoadTargets.cmake NAMESPACE LibLoad::)

include(GenerateExportHeader)
generate_export_header(load EXPORT_FILE_NAME include/load/export.hpp)
configure_file(include/load/version.hpp.cmake include/load/version.hpp)

target_include_directories(load PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/src ${Boost_INCLUDE_DIRS}
                                PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
                                        $<INSTALL_INTERFACE:include>)

install(DIRECTORY include/load DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN *.hpp)
install(FILES ${CMAKE_BINARY_DIR}/include/load/version.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/load)

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/LibLoadConfig.cmake LibLoadConfig.cmake
                              INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)
write_basic_package_version_file(LibLoadConfigVersion.cmake VERSION ${PROJECT_VERSION}
                                                            COMPATIBILITY AnyNewerVersion)

install(FILES ${CMAKE_BINARY_DIR}/LibLoadTargets.cmake
              ${CMAKE_BINARY_DIR}/LibLoadConfigVersion.cmake
              DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)

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
              DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake)

add_library(sample_module MODULE test/sample_module.cpp)
set_target_properties(sample_module PROPERTIES SUFFIX .llm)

add_executable(test_loadmodule test/test_loadmodule.cpp)
target_include_directories(test_loadmodule PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(test_loadmodule LibLoad::load ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

add_test(NAME LoadModule COMMAND "$<TARGET_FILE:test_loadmodule>"
                         WORKING_DIRECTORY "$<TARGET_FILE_DIR:sample_module>")