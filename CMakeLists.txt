cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)
project("myfs"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS OFF)


add_executable(main)

#扫描src目录下的所有源文件
file(GLOB_RECURSE SRC_FILES src/*.cpp src/*.h)

target_sources(main
  PRIVATE
  ${SRC_FILES}
  main.cpp
)