cmake_minimum_required(VERSION 3.20)
project(GameEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(app
  src/main.cc
  src/gfx/Context.cc
  src/gfx/Swapchain.cc)

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(app PRIVATE
  glfw
  Vulkan::Vulkan)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
  target_compile_options(app PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -pedantic-errors)
endif()
