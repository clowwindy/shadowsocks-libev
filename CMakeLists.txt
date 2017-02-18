cmake_minimum_required(VERSION 3.6)

set(PROJECT_NAME shadowsocks-libev)
set(RELEASE_DATE 2017-2-17)
set(PROJECT_VERSION 3.0.2)
project(${PROJECT_NAME})


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -c")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# -------------------------------------------------------------
# Options
option(BUILD_STATIC "Static link library to executable" ON)


if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()
# Detect linux
if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()


# -------------------------------------------------------------
# config.h

# If we generate config.h by automake
#include_directories(.)

# Use cmake to generate config.h
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)

# Define if building universal (internal helper macro)
# AC_APPLE_UNIVERSAL_BUILD
set(CONNECT_IN_PROGRESS "EINPROGRESS")
set(CONNECT_IN_PROGRESS "EINPROGRESS" CACHE STRING "")


check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(ev.h HAVE_EV_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_function_exists(fork HAVE_FORK)
check_function_exists(getpwnam_r HAVE_GETPWNAM_R)
check_function_exists(inet_ntop HAVE_INET_NTOP)
check_include_files(inttypes.h HAVE_INTTYPES_H)
set(HAVE_IPv6 1)
check_include_files(langinfo.h HAVE_LANGINFO_H)
set(HAVE_LIBPCRE 1)
check_library_exists(socket socket "" HAVE_LIBSOCKET)
check_include_files(limits.h HAVE_LIMITS_H)
check_include_files(linux/if.h HAVE_LINUX_IF_H)
check_include_files(linux/netfilter_ipv4.h HAVE_LINUX_NETFILTER_IPV4_H)
check_include_files(linux/netfilter_ipv6/ip6_tables.h HAVE_LINUX_NETFILTER_IPV6_IP6_TABLES_H)
check_include_files(locale.h HAVE_LOCALE_H)


check_function_exists(malloc HAVE_MALLOC)
check_include_files(memory.h HAVE_MEMORY_H)
check_function_exists(memset HAVE_MEMSET)

check_include_files(netdb.h HAVE_NETDB_H)
check_include_files(netinet/in.h HAVE_NETINET_IN_H)
check_include_files(net/if.h HAVE_NET_IF_H)
check_include_files(pcre.h HAVE_PCRE_H)
check_include_files(pcre/pcre.h HAVE_PCRE_PCRE_H)
check_symbol_exists(PTHREAD_PRIO_INHERIT pthread.h HAVE_PTHREAD_PRIO_INHERIT)

check_function_exists(select HAVE_SELECT)
check_function_exists(setresuid HAVE_SETRESUID)
check_function_exists(setreuid HAVE_SETREUID)
check_function_exists(setrlimit HAVE_SETRLIMIT)
check_function_exists(socket HAVE_SOCKET)

check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)

check_function_exists(strerror HAVE_STRERROR)

check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_files(sys/select.h HAVE_SYS_SELECT_H)
check_include_files(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(sys/wait.h HAVE_SYS_WAIT_H)
check_include_files(udns.h HAVE_UDNS_H)
check_include_files(unistd.h HAVE_UNISTD_H)

check_function_exists(fork HAVE_FORK)
check_function_exists(vfork HAVE_VFORK)
check_include_files(vfork.h HAVE_VFORK_H)
if (HAVE_VFORK)
    set(HAVE_WORKING_VFORK 1)
endif ()
if (HAVE_FORK)
    set(HAVE_WORKING_FORK 1)
endif ()


# Define to the sub-directory where libtool stores uninstalled libraries.
set(LT_OBJDIR ".libs/")
set(NDEBUG 1)
set(PACKAGE ${PROJECT_NAME})
set(PACKAGE_BUGREPORT max.c.lv@gmail.com)
set(PACKAGE_NAME ${PROJECT_NAME})
#set(PACKAGE_VERSION ${PROJECT_VERSION})
set(PACKAGE_VERSION 3.0.2)
set(PACKAGE_STRING "${PROJECT_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME ${PROJECT_NAME})
set(PACKAGE_URL "")

#message(${PACKAGE_NAME} - v${PACKAGE_VERSION} - v${PROJECT_VERSION})

# PTHREAD_CREATE_JOINABLE

# Define as the return type of signal handlers (`int' or `void').
set(RETSIGTYPE void)

# Define to the type of arg 1 for `select'.
set(SELECT_TYPE_ARG1 int)

# Define to the type of args 2, 3 and 4 for `select'.
set(SELECT_TYPE_ARG234 "(fd_set *)")

# Define to the type of arg 5 for `select'.
set(SELECT_TYPE_ARG5 "(struct timeval *)")

# Define to 1 if you have the ANSI C header files.
set(STDC_HEADERS 1)


check_include_files(sys/time.h time.h TIME_WITH_SYS_TIME)


# If the compiler supports a TLS storage class define it to that here
check_c_source_compiles("
        __thread int tls;
        int main(void) { return 0; }"
        HAVE_GCC_THREAD_LOCAL_STORAGE)
if (HAVE_GCC_THREAD_LOCAL_STORAGE)
    set(TLS __thread)
endif ()

set(_ALL_SOURCE 1)
set(_GNU_SOURCE 1)
set(_POSIX_PTHREAD_SEMANTICS 1)
set(_TANDEM_SOURCE 1)
set(__EXTENSIONS__ 1)
# USE_SYSTEM_SHARED_LIB
set(VERSION ${PACKAGE_VERSION})
# TODO WORDS_BIGENDIAN
# _MINIX
# _POSIX_1_SOURCE
# _POSIX_SOURCE
# _UINT8_T

# Define to empty if `const' does not conform to ANSI C.
# undef const

# Define to `__inline__' or `__inline' if that's what the C compiler
# calls it, or to nothing if 'inline' is not supported under any name.
#ifndef __cplusplus
#undef inline
#endif
# TODO Assume we got inline support
# https://cmake.org/Wiki/CMakeTestInline

# Define to the equivalent of the C99 'restrict' keyword, or to
# nothing if this is not supported.  Do not define if restrict is
# supported directly.
#define restrict __restrict
if (NOT "c_restrict" IN_LIST CMAKE_C_COMPILE_FEATURES)
    message("No restrict")
    set(restrict __restrict)
endif ()

# Define to `int' if <sys/types.h> does not define.
# undef pid_t
# Define to the type of an unsigned integer type of width exactly 16 bits if
# such a type exists and the standard includes do not define it.
# undef uint16_t
# Define to the type of an unsigned integer type of width exactly 8 bits if
# such a type exists and the standard includes do not define it.
# undef uint8_t
set(CMAKE_EXTRA_INCLUDE_FILES sys/types.h)
check_type_size(pid_t PID_T)
check_type_size(size_t SIZE_T)
check_type_size(ssize_t SSIZE_T)
set(CMAKE_EXTRA_INCLUDE_FILES)

check_type_size(uint16_t UINT16_T)
check_type_size(uint8_t UINT8_T)

## Inverse
if (NOT HAVE_PID_T)
    set(pid_t int)
endif ()
if (NOT HAVE_SIZE_T)
    set(size_t "unsigned int")
endif ()
if (NOT HAVE_SSIZE_T)
    set(ssize_t int)
endif ()

if (NOT HAVE_UINT8_T)
    set(uint8_t "unsigned char")
endif ()
if (NOT HAVE_UINT16_T)
    set(uint16_t "unsigned short")
endif ()


# Define as `fork' if `vfork' does not work.
if (NOT HAVE_WORKING_VFORK)
    set(vfork fork)
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

add_definitions(-DHAVE_CONFIG_H)

# -------------------------------------------------------------
# Source

set(SNI_SOURCE
        src/http.c
        src/tls.c
        src/rule.c
        )

set(CRYPTO_SOURCE
        src/crypto.c
        src/aead.c
        src/stream.c
        src/base64.c
        )

set(PLUGIN_SOURCE
        src/plugin.c
        )

set(SS_LOCAL_SOURCE
        src/utils.c
        src/jconf.c
        src/json.c
        src/netutils.c
        src/udprelay.c
        src/cache.c
        src/acl.c
        src/local.c
        ${CRYPTO_SOURCE}
        ${PLUGIN_SOURCE}
        ${SNI_SOURCE}
        )

set(SS_TUNNEL_SOURCE
        src/utils.c
        src/jconf.c
        src/json.c
        src/netutils.c
        src/udprelay.c
        src/cache.c
        src/tunnel.c
        ${CRYPTO_SOURCE}
        ${PLUGIN_SOURCE}
        )

set(SS_SERVER_SOURCE
        src/utils.c
        src/jconf.c
        src/json.c
        src/netutils.c
        src/udprelay.c
        src/cache.c
        src/acl.c
        src/resolv.c
        src/server.c
        ${CRYPTO_SOURCE}
        ${PLUGIN_SOURCE}
        ${SNI_SOURCE}
        )

set(SS_MANAGER_SOURCE
        src/utils.c
        src/jconf.c
        src/json.c
        src/netutils.c
        src/manager.c
        )

set(SS_REDIR_SOURCE
        src/utils.c
        src/jconf.c
        src/json.c
        src/netutils.c
        src/udprelay.c
        src/cache.c
        src/redir.c
        ${CRYPTO_SOURCE}
        ${PLUGIN_SOURCE}
        ${SNI_SOURCE}
        )

# We don't care about shared
set(ENABLE_SHARED OFF)
set(ENABLE_SHARED_EXECUTABLES OFF)
set(ENABLE_STATIC ON)
set(ENABLE_STATIC_EXECUTABLES ON)
#set(CMAKE_MACOSX_RPATH TRUE)

# We need libcork,libipset headers
include_directories(libcork/include)
include_directories(libipset/include)

# Things we need to build libcork,libipset
#link_directories(libcork/src)
#link_directories(libipset/src)

#add_subdirectory(libcork EXCLUDE_FROM_ALL)
# So libipset can find cork
#set( ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:libcork/src" )
#add_subdirectory(libipset EXCLUDE_FROM_ALL)

add_library(
        libcork STATIC
        libcork/src/libcork/cli/commands.c
        libcork/src/libcork/core/allocator.c
        libcork/src/libcork/core/error.c
        libcork/src/libcork/core/gc.c
        libcork/src/libcork/core/hash.c
        libcork/src/libcork/core/ip-address.c
        libcork/src/libcork/core/mempool.c
        libcork/src/libcork/core/timestamp.c
        libcork/src/libcork/core/u128.c
        libcork/src/libcork/core/version.c
        libcork/src/libcork/ds/array.c
        libcork/src/libcork/ds/bitset.c
        libcork/src/libcork/ds/buffer.c
        libcork/src/libcork/ds/dllist.c
        libcork/src/libcork/ds/file-stream.c
        libcork/src/libcork/ds/hash-table.c
        libcork/src/libcork/ds/managed-buffer.c
        libcork/src/libcork/ds/ring-buffer.c
        libcork/src/libcork/ds/slice.c
        libcork/src/libcork/posix/directory-walker.c
        libcork/src/libcork/posix/env.c
        libcork/src/libcork/posix/exec.c
        libcork/src/libcork/posix/files.c
        libcork/src/libcork/posix/process.c
        libcork/src/libcork/posix/subprocess.c
        libcork/src/libcork/pthreads/thread.c
)

target_compile_definitions(libcork PUBLIC -DCORK_API=CORK_LOCAL)


set(LIBIPSET_SRC
        libipset/src/libipset/general.c
        libipset/src/libipset/bdd/assignments.c
        libipset/src/libipset/bdd/basics.c
        libipset/src/libipset/bdd/bdd-iterator.c
        libipset/src/libipset/bdd/expanded.c
        libipset/src/libipset/bdd/reachable.c
        libipset/src/libipset/bdd/read.c
        libipset/src/libipset/bdd/write.c
        libipset/src/libipset/map/allocation.c
        libipset/src/libipset/map/inspection.c
        libipset/src/libipset/map/ipv4_map.c
        libipset/src/libipset/map/ipv6_map.c
        libipset/src/libipset/map/storage.c
        libipset/src/libipset/set/allocation.c
        libipset/src/libipset/set/inspection.c
        libipset/src/libipset/set/ipv4_set.c
        libipset/src/libipset/set/ipv6_set.c
        libipset/src/libipset/set/iterator.c
        libipset/src/libipset/set/storage.c
        )

add_library(libipset STATIC ${LIBIPSET_SRC})
set_target_properties(libipset PROPERTIES
        OUTPUT_NAME ipset
        VERSION 1.1.0
        SOVERSION 1)
target_link_libraries(libipset libcork)

# Add our targets
add_executable(ss-server ${SS_SERVER_SOURCE})
add_executable(ss-tunnel ${SS_TUNNEL_SOURCE})
add_executable(ss-manager ${SS_MANAGER_SOURCE})
add_executable(ss-local ${SS_LOCAL_SOURCE})

target_compile_definitions(ss-server PUBLIC -DMODULE_REMOTE)
target_compile_definitions(ss-tunnel PUBLIC -DMODULE_TUNNEL)
target_compile_definitions(ss-manager PUBLIC -DMODULE_MANAGER)
target_compile_definitions(ss-local PUBLIC -DMODULE_LOCAL)


if (BUILD_STATIC)
    message("Build static linked executable")
    set(BUILD_SHARED_LIBS OFF)
    find_library(LIBSODIUM libsodium.a)
    find_library(LIBMBEDTLS libmbedtls.a)
    find_library(LIBMBEDCRYPTO libmbedcrypto.a)
    find_library(LIBEV libev.a)
    find_library(LIBUDNS libudns.a)
    find_library(LIBPCRE libpcre.a)

    # Clang crt0 issues
    # https://github.com/skaht/Csu-85
    # https://bugs.llvm.org//show_bug.cgi?id=17801
    # set(CMAKE_EXE_LINKER_FLAGS "-static")
else ()
    message("Build shared linked executable")

    find_library(LIBSODIUM sodium)
    find_library(LIBMBEDTLS mbedtls)
    find_library(LIBMBEDCRYPTO mbedcrypto)
    find_library(LIBEV ev)
    find_library(LIBUDNS udns)
    find_library(LIBPCRE pcre)
endif ()

list(APPEND DEPS ${LIBEV} ${LIBUDNS} ${LIBPCRE} ${LIBSODIUM} ${LIBMBEDTLS} ${LIBMBEDCRYPTO})

target_link_libraries(ss-server libcork libipset ${DEPS})
target_link_libraries(ss-tunnel libcork ${DEPS})
target_link_libraries(ss-manager libcork ${LIBEV} ${LIBUDNS})
target_link_libraries(ss-local libcork libipset ${DEPS})

# redir need linux/* stuff
if (LINUX)
    add_executable(ss-redir ${SS_REDIR_SOURCE})
    target_compile_definitions(ss-redir PUBLIC -DMODULE_REDIR)
    target_link_libraries(ss-redir libcork libipset ${DEPS})
endif (LINUX)

install(DIRECTORY DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install(TARGETS ss-server RUNTIME DESTINATION bin)
install(TARGETS ss-tunnel RUNTIME DESTINATION bin)
install(TARGETS ss-manager RUNTIME DESTINATION bin)
install(TARGETS ss-local RUNTIME DESTINATION bin)

if (LINUX)
    install(TARGETS ss-redir RUNTIME DESTINATION bin)
endif (LINUX)
