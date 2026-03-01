cmake_minimum_required(VERSION 3.21)

# set the project name
project(m1VulkanEngine VERSION 0.1.0 LANGUAGES C CXX)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

# Core SDK dependency
find_package(Vulkan REQUIRED)

# Third-party dependencies

# glfw
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# glm
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)

# tinyobjloader
FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG v2.0.0rc13
)
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
set(TINYOBJLOADER_INSTALL OFF CACHE BOOL "" FORCE)

# Vulkan Memory Allocator
FetchContent_Declare(
  vma
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v3.1.0
)

# stb
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

# fastgltf
FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY https://github.com/spnda/fastgltf.git
  GIT_TAG v0.9.0
)

# imgui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.6
)

FetchContent_MakeAvailable(glfw glm tinyobjloader vma stb fastgltf imgui)

# Create a static library for ImGui because is not ship with its own CMakeLists.txt
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  # Backends for GLFW + Vulkan
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC
  glfw
  Vulkan::Vulkan
)

# Find all .cpp files in the specified directories and its subdirectories
file(GLOB_RECURSE SOURCES
  ${PROJECT_SOURCE_DIR}/src/*.cpp
)

# Create an executable target from the collected source files
add_executable(${PROJECT_NAME} ${SOURCES})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# define DEBUG when it's Debug
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

# specify include directories to use when compiling
target_include_directories(${PROJECT_NAME} PUBLIC
  ${PROJECT_SOURCE_DIR}/src
  ${stb_SOURCE_DIR}
  ${vma_SOURCE_DIR}/include
)

# specifies library to use when linking
target_link_libraries(${PROJECT_NAME} PRIVATE
  glfw
  Vulkan::Vulkan
  tinyobjloader
  imgui
)

if (TARGET glm::glm)
  target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
endif()

if (TARGET fastgltf::fastgltf)
  target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)
else()
  target_include_directories(${PROJECT_NAME} PUBLIC ${fastgltf_SOURCE_DIR}/include)
endif()

############## Build SHADERS #######################

file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/shaders/compiled)

# Find all vertex and fragment sources within shaders directory
# taken from VBlancos vulkan tutorial
# https://github.com/vblanco20-1/vulkan-guide/blob/all-chapters/CMakeLists.txt
find_program(GLSL_VALIDATOR glslangValidator HINTS
  ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
  /usr/bin
  /usr/local/bin
  $ENV{VULKAN_SDK}/Bin/
  $ENV{VULKAN_SDK}/Bin32/
)

# get all .vert and .frag files in shaders directory
file(GLOB_RECURSE GLSL_SOURCE_FILES
  "${PROJECT_SOURCE_DIR}/shaders/*.frag"
  "${PROJECT_SOURCE_DIR}/shaders/*.vert"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/compiled/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL}
  )
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
