cmake_minimum_required(VERSION 3.14)

project(pws)

set(PWS_VERSION 0.1.0)

file(GLOB_RECURSE PWS_SOURCES "src/**.c" "src/**.h" "include/**.h*")
add_library(pws STATIC ${PWS_SOURCES})

target_include_directories(pws PUBLIC
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

set_property(TARGET pws PROPERTY C_STANDARD 99)

target_compile_definitions(pws PRIVATE
  $<$<PLATFORM_ID:Windows>:PWS_PLATFORM_WINDOWS>
)

target_compile_options(pws PRIVATE
  $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-Wall -Wextra -Werror -Wpedantic>
  $<$<AND:$<CONFIG:Debug>,$<C_COMPILER_ID:MSVC>>:/W4 /WX>
)

# add_subdirectory(example/)



include(GNUInstallDirs)
set(PWS_CONFIG_PATH "${CMAKE_INSTALL_LIBDIR}/cmake/pws")

include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/pwsConfig.cmake.in
                              src/pwsConfig.cmake
                              INSTALL_DESTINATION "${PWS_CONFIG_PATH}"
                              NO_CHECK_REQUIRED_COMPONENTS_MACRO)

write_basic_package_version_file(src/pwsConfigVersion.cmake
                                 VERSION ${PWS_VERSION}
                                 COMPATIBILITY SameMajorVersion)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/src/pwsConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/src/pwsConfigVersion.cmake"
        DESTINATION "${PWS_CONFIG_PATH}")

install(EXPORT pwsTargets
  FILE pwsTargets.cmake
  NAMESPACE pws::
  DESTINATION ${PWS_CONFIG_PATH}
)

install(TARGETS pws
        EXPORT pwsTargets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})