cmake_minimum_required(VERSION 3.20)
project(vulkan_bandwidth LANGUAGES CXX)

find_package(Vulkan REQUIRED COMPONENTS glslc)

set(SHADER_SPV "${CMAKE_CURRENT_BINARY_DIR}/copy.comp.spv")
add_custom_command(
  OUTPUT "${SHADER_SPV}"
  COMMAND Vulkan::glslc "${CMAKE_CURRENT_SOURCE_DIR}/copy.comp" -O -o "${SHADER_SPV}"
  DEPENDS copy.comp
  VERBATIM
)

add_executable(vulkan_bandwidth main.cpp "${SHADER_SPV}")
target_compile_features(vulkan_bandwidth PRIVATE cxx_std_20)
target_compile_options(vulkan_bandwidth PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
target_link_libraries(vulkan_bandwidth PRIVATE Vulkan::Vulkan)
target_compile_definitions(vulkan_bandwidth PRIVATE SHADER_PATH="${SHADER_SPV}")
