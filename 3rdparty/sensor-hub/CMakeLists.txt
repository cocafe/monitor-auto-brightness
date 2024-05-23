cmake_minimum_required(VERSION 3.18)

set(PROJECT_NAME sensor-hub)

project(${PROJECT_NAME} C)

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_C_STANDARD 11)

set(LIBJJ_HAVE_CJSON 1)
#set(LIBJJ_HAVE_SOCKET 1)
set(LIBJJ_HAVE_ICONV 1)
set(LIBJJ_HAVE_MATH 1)
#set(LIBJJ_HAVE_UUID 1)

add_executable(${PROJECT_NAME})

set(SOURCE_FILES
    src/main.c
    src/prometheus.h
    lib/simple_uart/simple_uart.c
    lib/simple_uart/simple_uart.h
    )

target_sources(${PROJECT_NAME} PRIVATE ${SOURCE_FILES})

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
target_compile_options(${PROJECT_NAME} PRIVATE -Wextra)
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-parameter)
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-function)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        target_compile_options(${PROJECT_NAME} PRIVATE -g3)
        target_compile_options(${PROJECT_NAME} PRIVATE -ggdb3)
        target_compile_options(${PROJECT_NAME} PRIVATE -gdwarf-4)
        target_compile_options(${PROJECT_NAME} PRIVATE -O0)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
        target_compile_options(${PROJECT_NAME} PRIVATE -O2)
endif()

add_subdirectory(lib/libjj)

target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PUBLIC lib)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} jjcom)

set(INSTALL_DEST "Build-${CMAKE_BUILD_TYPE}")

install(TARGETS ${PROJECT_NAME} DESTINATION "${INSTALL_DEST}")

message(STATUS "CMake Config: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: " ${CMAKE_C_COMPILER})
message(STATUS "C Flags: ${CMAKE_C_FLAGS} ")
message(STATUS "Source Directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "External Object: ${APPRES_OBJS}")
message(STATUS "Install destination: " ${INSTALL_DEST})
