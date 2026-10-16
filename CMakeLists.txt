cmake_minimum_required(VERSION 3.13)
project(imbe-mod2 C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

file(GLOB IMBE2_SRC src/*.cc)

add_library(imbe2 STATIC ${IMBE2_SRC})
target_include_directories(imbe2 PUBLIC inc)
# integer-only engine: optimization flags cannot change numerics
target_compile_options(imbe2 PRIVATE -O3 -Wall -Wextra -flto=auto)
target_link_options(imbe2 INTERFACE -flto=auto)

add_executable(imbe_enc tools/imbe_enc.c)
add_executable(imbe_enc2 tools/imbe_enc2.c)
target_link_libraries(imbe_enc PRIVATE imbe2)
target_link_libraries(imbe_enc2 PRIVATE imbe2)

add_executable(imbe_dec tools/imbe_dec.c)
target_link_libraries(imbe_dec PRIVATE imbe2)

add_executable(imbe2test tests/imbe2test.c)
target_link_libraries(imbe2test PRIVATE imbe2 m)

enable_testing()
add_test(NAME imbe2test COMMAND imbe2test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
