cmake_minimum_required(VERSION 3.18)

set(LIBJJ_NAME jjcom)

project(${LIBJJ_NAME} C)

set(CMAKE_C_STANDARD 11)

set(COMMON_SRC
    bits.h
    opts.c
    opts.h
    ffs.h
    file.c
    file.h
    hash.h
    hashtable.h
    list.h
    list_sort.h
    log2.h
    logging.c
    logging.h
    malloc.c
    malloc.h
    string.c
    string.h
    utils.h
    uuid.h
    endian_wrap.h
    )

if (MINGW OR WINNT)
        set(GETOPT_SRC getopt_musl.h getopt_musl.c)
endif()

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
        message(STATUS "Build type is not defined, set default one")
        set(CMAKE_BUILD_TYPE "Debug")
endif()

add_library(${LIBJJ_NAME} ${COMMON_SRC})

if ("${LIBJJ_HAVE_SOCKET}" STREQUAL "1")
        target_compile_definitions(${LIBJJ_NAME} PRIVATE -DHAVE_SOCKET)

        if (WIN32 AND MINGW)
                set(SOCKET_SRC wsock2.c socket.h)
                set(SOCKET_LIB ws2_32)
        else()
                set(SOCKET_SRC socket.c socket.h)
                set(SOCKET_LIB )
        endif()
endif()

if ("${LIBJJ_HAVE_ICONV}" STREQUAL "1")
        target_compile_definitions(${LIBJJ_NAME} PRIVATE -DHAVE_ICONV)
        set(ICONV_SRC iconv.h iconv_wrap.c iconv.c)
        if (WINNT)
                set(ICONV_LIB iconv)
        endif()
else()
        set(ICONV_SRC iconv.h iconv_wrap.c)
endif()

if ("${LIBJJ_HAVE_TRAY}" STREQUAL "1")
        target_compile_definitions(${LIBJJ_NAME} PRIVATE -DHAVE_TRAY)
        set(TRAY_SRC tray.c tray.h)
endif()

if ("${LIBJJ_HAVE_MATH}" STREQUAL "1")
        target_compile_definitions(${LIBJJ_NAME} PRIVATE -DHAVE_MATH)
        set(MATH_SRC math.c math.h)
        set(MATH_LIB m)
endif()

if ("${LIBJJ_HAVE_UUID}" STREQUAL "1")
        if (LINUX OR CYGWIN)
                target_compile_definitions(${LIBJJ_NAME} PRIVATE -DHAVE_UUID)
                set(UUID_SRC uuid.h)
                set(UUID_LIB uuid)
        endif()
endif()

if ("${LIBJJ_HAVE_CJSON}" STREQUAL "1")
        target_compile_definitions(${LIBJJ_NAME} PRIVATE -DHAVE_CJSON)
        set(JSON_SRC cJSON.c cJSON.h jkey.c jkey.h json.c json.h)
endif()



if ("${LIBJJ_WIN32_WINDOW}" STREQUAL "1")
        target_compile_definitions(${LIBJJ_NAME} PRIVATE -DSUBSYS_WINDOW)
endif()

if (WIN32 AND MINGW)
        if ("${LIBJJ_WIN32_UNICODE}" STREQUAL "1")
                target_compile_options(${LIBJJ_NAME} PRIVATE -municode)
                target_compile_definitions(${LIBJJ_NAME} PRIVATE -DUNICODE)
                target_compile_definitions(${LIBJJ_NAME} PRIVATE -D_UNICODE)
                target_link_options(${LIBJJ_NAME} PRIVATE -municode)
        endif()
endif()

target_compile_options(${LIBJJ_NAME} PRIVATE -Wall)
target_compile_options(${LIBJJ_NAME} PRIVATE -Wextra)
# target_compile_options(${LIBJJ_NAME} PRIVATE -Wno-unused-parameter)
# target_compile_options(${LIBJJ_NAME} PRIVATE -Wno-unused-function)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
        target_compile_options(${LIBJJ_NAME} PRIVATE -g3)
        target_compile_options(${LIBJJ_NAME} PRIVATE -ggdb3)
        target_compile_options(${LIBJJ_NAME} PRIVATE -gdwarf-4)
        target_compile_options(${LIBJJ_NAME} PRIVATE -O0)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Release")
        target_compile_options(${LIBJJ_NAME} PRIVATE -O2)
endif()

target_sources(${LIBJJ_NAME} PRIVATE ${JSON_SRC} ${SOCKET_SRC} ${ICONV_SRC} ${MATH_SRC} ${UUID_SRC} ${GETOPT_SRC} ${TRAY_SRC})
target_link_libraries(${LIBJJ_NAME} pthread ${ICONV_LIB} ${MATH_LIB} ${UUID_LIB} ${SOCKET_LIB})
