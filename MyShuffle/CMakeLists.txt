cmake_minimum_required(VERSION 3.5.0)
project(Multi-partyShuffle VERSION 0.1.0 LANGUAGES C CXX)

add_executable(Multi-partyShuffle main.cpp)

include(CTest)
enable_testing()

target_link_libraries(${PROJECT_NAME} ${OPENSSL_LIBRARIES} ${EMP-SH2PC_LIBRARIES})

find_package(emp-tool REQUIRED)
include_directories(${EMP-TOOL_INCLUDE_DIRS})

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} OpenSSL::Crypto)

set(CMAKE_CXX_FLAGS "-std=c++20 -march=native")

add_custom_target(run
    COMMAND ./Multi-partyShuffle 10
    DEPENDS ./Multi-partyShuffle
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)

target_sources(${PROJECT_NAME} PRIVATE
    "double_length_prg.cpp"
    "field_int.cpp"
    "OPV.cpp"
    "mpc_communicator.cpp"
    "Chase_shuffle.cpp"
    "Benes_network.cpp"
    "math_gadget.cpp"
    "test_shuffle.cpp"
    "permutation.cpp"
    "Song_shuffle.cpp"
    "mpc_gadget.cpp")
