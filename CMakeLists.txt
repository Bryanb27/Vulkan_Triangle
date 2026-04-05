cmake_minimum_required(VERSION 3.10)
project(VulkanTriangle)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)

add_executable(triangle main.cpp)

target_include_directories(triangle PRIVATE ${GLFW_INCLUDE_DIRS})
target_link_libraries(triangle Vulkan::Vulkan ${GLFW_LIBRARIES})