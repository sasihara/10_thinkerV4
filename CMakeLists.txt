cmake_minimum_required(VERSION 3.14)
project(thinkerV3_PyTorch)

set(CMAKE_CXX_STANDARD 20)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CUDA_COMPILER "/usr/local/cuda/bin/nvcc")
set(USE_CUDA OFF CACHE BOOL "" FORCE)
list(APPEND CMAKE_PREFIX_PATH "/usr/local/lib/python3.12/dist-packages/torch")
find_package(Torch REQUIRED)
find_package(OpenMP REQUIRED COMPONENTS CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS} -O3 -march=native")

include_directories(
    01_othello/othello
    01_othello/common
    ${TORCH_INCLUDE_DIRS}
)

add_executable(thinkerV4pt_arm
    10_thinkerV4/main.cpp
    10_thinkerV4/history.cpp
    10_thinkerV4/Node.cpp
    10_thinkerV4/PTHHandler.cpp
    10_thinkerV4/Pv_mcts_action.cpp
    10_thinkerV4/State.cpp
    10_thinkerV4/think.cpp
    10_thinkerV4/thinkV1.cpp
    01_othello/common/logging.cpp
    01_othello/common/messageGenerator.cpp
    01_othello/common/messageParser.cpp
)

target_compile_definitions(thinkerV4pt_arm PRIVATE LOGLEVEL=LOGLEVEL_INFO)

target_link_libraries(thinkerV4pt_arm "${TORCH_LIBRARIES}" OpenMP::OpenMP_CXX)
