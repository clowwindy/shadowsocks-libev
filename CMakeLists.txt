cmake_minimum_required(VERSION 3.6)

set(PROJECT_NAME shadowsocks_libev)
set(RELEASE_DATE 2017-2-17)
project(${PROJECT_NAME})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -c")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
#set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif ()
# Detect linux
if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()


# for config.h
include_directories(.)
add_definitions(-DHAVE_CONFIG_H)

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

# redir need linux/* stuff
if (LINUX)
    add_executable(ss-redir ${SS_REDIR_SOURCE})
    target_compile_definitions(ss-redir PUBLIC -DMODULE_REDIR)
    target_link_libraries(ss-redir cork ev sodium mbedtls mbedcrypto libipset udns pcre)
endif (LINUX)

target_link_libraries(ss-server libcork ev sodium mbedtls mbedcrypto libipset udns pcre)
target_link_libraries(ss-tunnel libcork ev sodium mbedtls mbedcrypto libipset udns pcre)
target_link_libraries(ss-manager libcork ev udns)
target_link_libraries(ss-local libcork ev sodium mbedtls mbedcrypto libipset udns pcre)

install(DIRECTORY DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install(TARGETS ss-server RUNTIME DESTINATION bin)
install(TARGETS ss-tunnel RUNTIME DESTINATION bin)
install(TARGETS ss-manager RUNTIME DESTINATION bin)
install(TARGETS ss-local RUNTIME DESTINATION bin)