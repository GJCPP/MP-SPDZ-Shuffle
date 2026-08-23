cmake_minimum_required(VERSION 3.16)

project(MP_SPDZ_Shuffle LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(USE_GF2N_LONG "Match the default MP-SPDZ CONFIG setting" ON)

add_compile_options(-march=native -g -O3 -Wall -Wextra -Werror=return-type -pthread)

if(USE_GF2N_LONG)
    add_compile_definitions(USE_GF2N_LONG)
endif()

add_compile_definitions(
    PREP_DIR="Player-Data/"
    SSL_DIR="Player-Data/"
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

include_directories(SYSTEM
    ${CMAKE_CURRENT_SOURCE_DIR}/deps
    ${CMAKE_CURRENT_SOURCE_DIR}/local/include
)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/local/lib)

file(GLOB MATH_SRC CONFIGURE_DEPENDS Math/*.cpp)
file(GLOB TOOLS_SRC CONFIGURE_DEPENDS Tools/*.cpp)
file(GLOB NETWORKING_SRC CONFIGURE_DEPENDS Networking/*.cpp)
file(GLOB PROCESSOR_SRC CONFIGURE_DEPENDS Processor/*.cpp)
file(GLOB FHE_SRC CONFIGURE_DEPENDS FHE/*.cpp)
file(GLOB FHEOFFLINE_SRC CONFIGURE_DEPENDS FHEOffline/*.cpp)
file(GLOB OT_SRC CONFIGURE_DEPENDS OT/*.cpp)
file(GLOB MYSHUFFLE_SRC CONFIGURE_DEPENDS MyShuffle/*.cpp)

set(SPDZ_RUNTIME_SRC
    ${PROCESSOR_SRC}
    ${MATH_SRC}
    ${TOOLS_SRC}
    ${NETWORKING_SRC}
    GC/square64.cpp
    GC/Instruction.cpp
)

add_library(SPDZRuntime SHARED ${SPDZ_RUNTIME_SRC})
target_link_libraries(SPDZRuntime
    gmpxx gmp sodium
    boost_system ssl crypto boost_filesystem boost_iostreams rt
)
set_target_properties(SPDZRuntime PROPERTIES OUTPUT_NAME SPDZ)

add_library(FHEOfflineRuntime SHARED
    ${FHEOFFLINE_SRC}
    ${FHE_SRC}
    Protocols/CowGearOptions.cpp
)
target_link_libraries(FHEOfflineRuntime
    SPDZRuntime
    gmpxx gmp sodium
    boost_system ssl crypto boost_filesystem boost_iostreams rt
)
set_target_properties(FHEOfflineRuntime PROPERTIES OUTPUT_NAME FHE)

add_executable(my_shuffle_main
    ${MYSHUFFLE_SRC}
    ${OT_SRC}
)

target_link_libraries(my_shuffle_main
    SPDZRuntime
    FHEOfflineRuntime
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/SimplestOT_C/ref10/libSimplestOT.a
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/SimpleOT/libsimpleot.a
    ${CMAKE_CURRENT_SOURCE_DIR}/local/lib/liblibOTe.a
    ${CMAKE_CURRENT_SOURCE_DIR}/local/lib/libcryptoTools.a
    gmpxx gmp sodium
    boost_system ssl crypto boost_filesystem boost_iostreams rt
    pthread
)

set_target_properties(my_shuffle_main PROPERTIES
    OUTPUT_NAME my_shuffle_main.x
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    BUILD_RPATH "${CMAKE_BINARY_DIR};${CMAKE_CURRENT_SOURCE_DIR}/local/lib"
)
