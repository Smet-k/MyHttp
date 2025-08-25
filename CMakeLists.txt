cmake_minimum_required(VERSION 3.14.0)
project(MyHttp LANGUAGES C)

add_compile_options(-Werror)
set(CMAKE_C_STANDARD 11)

file(GLOB MyHttp_src "src/*.c")
add_executable(MyHttp ${MyHttp_src})
target_include_directories(MyHttp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)