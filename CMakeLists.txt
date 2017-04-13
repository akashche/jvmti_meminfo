
cmake_minimum_required ( VERSION 2.8.12 )

# toolchain setup
if ( NOT DEFINED STATICLIB_TOOLCHAIN )
    if ( WIN32 )
        if ( CMAKE_SIZEOF_VOID_P EQUAL 4 )
            set ( STATICLIB_TOOLCHAIN "windows_i386_msvc" )
        else ( )
            set ( STATICLIB_TOOLCHAIN "windows_amd64_msvc" )
        endif ( )
    elseif ( APPLE )
        set ( STATICLIB_TOOLCHAIN "macosx_amd64_clang" )
    elseif ( UNIX )
        set ( STATICLIB_TOOLCHAIN "linux_amd64_gcc" )
    else ( )
        message ( FATAL_ERROR "Cannot determine toolchain to use, please specify [STATICLIB_TOOLCHAIN]" )
    endif ( )
endif ( )
message ( STATUS "Using toolchain: [${STATICLIB_TOOLCHAIN}]" )
#set ( STATICLIB_CMAKE ${CMAKE_CURRENT_LIST_DIR}/cmake CACHE INTERNAL "" )    
set ( STATICLIB_CMAKE /home/alex/projects/staticlibs/cmake/ CACHE INTERNAL "" )    
set ( CMAKE_TOOLCHAIN_FILE ${STATICLIB_CMAKE}/toolchains/${STATICLIB_TOOLCHAIN}.cmake CACHE INTERNAL "" )

# project setup
project ( meminfo CXX )
set ( ${PROJECT_NAME}_STATICLIB_VERSION 0.1 )
include ( ${STATICLIB_CMAKE}/staticlibs_common.cmake )

# options
set ( JAVA_HOME $ENV{JAVA_HOME} CACHE STRING "Path to JDK directory" )
#set ( STATICLIB_DEPS ${CMAKE_CURRENT_LIST_DIR}/deps CACHE INTERNAL "" )    
set ( STATICLIB_DEPS /home/alex/projects/staticlibs/ CACHE INTERNAL "" )    

# dependencies
staticlib_add_subdirectory ( ${STATICLIB_DEPS}/staticlib_config )
staticlib_add_subdirectory ( ${STATICLIB_DEPS}/staticlib_support )
staticlib_add_subdirectory ( ${STATICLIB_DEPS}/staticlib_concurrent )
staticlib_add_subdirectory ( ${STATICLIB_DEPS}/staticlib_io )
staticlib_add_subdirectory ( ${STATICLIB_DEPS}/staticlib_utils )
staticlib_add_subdirectory ( ${STATICLIB_DEPS}/staticlib_tinydir )

# pkg-config
set ( ${PROJECT_NAME}_DEPS
        staticlib_config
        staticlib_support
        staticlib_concurrent
        staticlib_io
        staticlib_tinydir )
staticlib_pkg_check_modules ( ${PROJECT_NAME}_DEPS_PC REQUIRED ${PROJECT_NAME}_DEPS )

# sources
file ( GLOB_RECURSE ${PROJECT_NAME}_SRC ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp )
file ( GLOB_RECURSE ${PROJECT_NAME}_HEADERS ${CMAKE_CURRENT_LIST_DIR}/include/*.hpp )
add_library ( ${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SRC} ${${PROJECT_NAME}_HEADERS} )
source_group ( "src" FILES ${${PROJECT_NAME}_SRC} )
source_group ( "include" FILES ${${PROJECT_NAME}_HEADERS} )

#includes
target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE 
        ${CMAKE_CURRENT_LIST_DIR}/src 
        ${${PROJECT_NAME}_DEPS_PC_INCLUDE_DIRS}
        ${JAVA_HOME}/include )
if ( STATICLIB_TOOLCHAIN MATCHES "windows_[^_]+_[^_]+" )
    target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE ${JAVA_HOME}/include/win32 )
elseif ( STATICLIB_TOOLCHAIN MATCHES "linux_[^_]+_[^_]+" )
    target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE ${JAVA_HOME}/include/linux )
elseif ( STATICLIB_TOOLCHAIN MATCHES "macosx_[^_]+_[^_]+" )
    target_include_directories ( ${PROJECT_NAME} BEFORE PRIVATE ${JAVA_HOME}/include/macosx )
endif ( )

# libs and other flags
target_link_libraries ( ${PROJECT_NAME} ${${PROJECT_NAME}_DEPS_PC_STATIC_LIBRARIES} )
if ( STATICLIB_TOOLCHAIN MATCHES "linux_[^_]+_[^_]+" )
    target_link_libraries ( ${PROJECT_NAME} pthread )
endif ( )
target_compile_options ( ${PROJECT_NAME} PRIVATE ${${PROJECT_NAME}_DEPS_PC_CFLAGS_OTHER} )

# test
enable_testing ( )
set ( ${PROJECT_NAME}_TARGET ${CMAKE_SHARED_LIBRARY_PREFIX}${PROJECT_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX} )
message ( STATUS "Compiling test java app" )
execute_process ( 
        COMMAND ${JAVA_HOME}/bin/javac -d . ../test/App.java
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
message ( STATUS "Compiling test java app - done" )
add_test ( test ${JAVA_HOME}/bin/java -agentpath:bin/${${PROJECT_NAME}_TARGET} App )
