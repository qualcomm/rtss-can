cmake_minimum_required(VERSION 3.16)

# Project definition
project(rtss_can
    VERSION 1.0.0
    DESCRIPTION "RTSS CAN Daemon - SocketCAN to RTSS Mailbox Bridge"
    LANGUAGES C
)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -DDEBUG -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG -Wall")

# Project options
option(BUILD_DOCS "Build documentation" OFF)

# Platform-specific settings
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_definitions(-D_GNU_SOURCE)
endif()

# Stub driver configuration
if(USE_STUB_DRIVERS)
    add_definitions(-DUSE_STUB_DRIVERS=1)
    message(STATUS "RTSS CAN: Using stub drivers")
else()
    add_definitions(-DUSE_STUB_DRIVERS=0)
    message(STATUS "RTSS CAN: Using real RTSS MB drivers")
endif()

# RTSS CAN specific configuration from build config
if(DEFINED RTSS_CAN_ENABLE_CANFD)
    if(RTSS_CAN_ENABLE_CANFD)
        add_definitions(-DENABLE_CANFD=1)
    else()
        add_definitions(-DENABLE_CANFD=0)
    endif()
endif()

if(DEFINED RTSS_CAN_ENABLE_GATEWAY)
    if(RTSS_CAN_ENABLE_GATEWAY)
        add_definitions(-DENABLE_GATEWAY=1)
    else()
        add_definitions(-DENABLE_GATEWAY=0)
    endif()
endif()

if(DEFINED RTSS_CAN_MAX_CONTROLLERS)
    add_definitions(-DMAX_CAN_CONTROLLERS=${RTSS_CAN_MAX_CONTROLLERS})
endif()

# Check for required system libraries
find_library(RT_LIBRARY rt)
if(NOT RT_LIBRARY)
    message(FATAL_ERROR "librt not found")
endif()

# Check for required headers
include(CheckIncludeFile)
check_include_file("linux/can.h" HAVE_LINUX_CAN_H)
check_include_file("linux/can/raw.h" HAVE_LINUX_CAN_RAW_H)

if(NOT HAVE_LINUX_CAN_H OR NOT HAVE_LINUX_CAN_RAW_H)
    message(FATAL_ERROR "Linux CAN headers not found. Please install can-utils-dev or kernel-headers")
endif()

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${SYSROOTINC_PATH})
include_directories(${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/include)
include_directories(${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/src)

include_directories(${SYSROOT_INCLUDEDIR})

# Build sail-mailbox library locally
# Define sail-mailbox source files
set(RTSS_MAILBOX_SOURCES
    ${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/src/sail_mbox_core.c
    ${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/src/sail_mbox_lib.c
)

# Create sail_umd static library from sail-mailbox sources
add_library(sail_umd STATIC ${RTSS_MAILBOX_SOURCES})

# Set include directories for the sail_umd library
target_include_directories(sail_umd PUBLIC
    ${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/include
    ${WORKDIR}/sail-mailbox/sail-mailbox-usr/sail-mb-umd/src
)

# Source files for the main daemon
set(DAEMON_SOURCES
    rtss_can.c
    rtss_can_logging.c
    rtss_mb_api.c
    # rtss_can_baud_configs.c - Excluded due to AUTOSAR dependencies
)

# Header files
set(DAEMON_HEADERS
    rtss_can_config.h
    rtss_can_structures.h
    rtss_can_logging.h
    rtss_mb_api.h
    rtss_can_baud_config.h
)

# Create the main executable
add_executable(rtss_can ${DAEMON_SOURCES})

# Link libraries
# Find glib-2.0 using pkg-config if available
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(GLIB2 QUIET glib-2.0)
    if(GLIB2_FOUND)
        set(GLIB2_LIBRARIES ${GLIB2_LIBRARIES})
        set(GLIB2_INCLUDE_DIRS ${GLIB2_INCLUDE_DIRS})
    else()
        set(GLIB2_LIBRARIES "glib-2.0")
    endif()
else()
    set(GLIB2_LIBRARIES "glib-2.0")
endif()

# Link rtss_can with the locally-built sail_umd library and other dependencies
target_link_libraries(rtss_can
    rtss_umd          # Locally-built static library
    ${RT_LIBRARY}
    pthread
    ${GLIB2_LIBRARIES}
)

# Add glib include directories if found
if(GLIB2_INCLUDE_DIRS)
    target_include_directories(rtss_can PRIVATE ${GLIB2_INCLUDE_DIRS})
endif()

# Compiler-specific options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(rtss_can PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wformat=2
        -Wformat-security
        -Wnull-dereference
        -Wstack-protector
        -Wtrampolines
        -Walloca
        -Wvla
        -Warray-bounds=2
        -Wimplicit-fallthrough=3
        -Wtraditional-conversion
        -Wshift-overflow=2
        -Wcast-qual
        -Wstringop-overflow=4
        -Wconversion
        -Warith-conversion
        -Wlogical-op
        -Wduplicated-cond
        -Wduplicated-branches
        -Wformat-overflow=2
        -Wformat-truncation=2
        -Wstringop-truncation
        -Wcast-align=strict
    )
endif()

# Security hardening flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(rtss_can PRIVATE
        _FORTIFY_SOURCE=2
    )
    
    target_compile_options(rtss_can PRIVATE
        -fstack-protector-strong
        -fPIE
    )
    
    target_link_options(rtss_can PRIVATE
        -pie
        -Wl,-z,relro
        -Wl,-z,now
        -Wl,-z,noexecstack
    )
endif()

# Installation
include(GNUInstallDirs)

# Install the main executable
install(TARGETS rtss_can
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install configuration files
install(FILES rtss_can.conf
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/rtss_can
)

# Package configuration
set(CPACK_PACKAGE_NAME "rtss-can")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_VENDOR "RTSS CAN Project")

# DEB package configuration
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, can-utils")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM package configuration
set(CPACK_RPM_PACKAGE_GROUP "System Environment/Daemons")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc, can-utils")

include(CPack)

# Documentation
if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        add_subdirectory(docs)
    else()
        message(WARNING "Doxygen not found, documentation will not be built")
    endif()
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "RTSS CAN Daemon Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Build docs: ${BUILD_DOCS}")
message(STATUS "")
