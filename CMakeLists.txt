cmake_minimum_required(VERSION 3.16)

project(FramedDataFormatToolkit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fdf STATIC
  src/format.cpp
  src/decoder.cpp
  src/parser.cpp
  src/deserializer.cpp
  src/validator.cpp
)

target_include_directories(fdf PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
  target_compile_options(fdf PRIVATE /W4)
else()
  target_compile_options(fdf PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(fdf_inspect src/main.cpp)
target_link_libraries(fdf_inspect PRIVATE fdf)

enable_testing()

add_executable(test_parser tests/test_parser.cpp)
target_link_libraries(test_parser PRIVATE fdf)
add_test(NAME parser COMMAND test_parser)

add_executable(test_decoder tests/test_decoder.cpp)
target_link_libraries(test_decoder PRIVATE fdf)
add_test(NAME decoder COMMAND test_decoder)
