cmake_minimum_required(VERSION 3.21)
project(m1VulkanEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

message(STATUS "Configuring ${PROJECT_NAME} ${PROJECT_VERSION}")

# Core SDK dependency
find_package(Vulkan QUIET)

if (NOT Vulkan_FOUND)
  message(STATUS "System Vulkan not found, fetching Vulkan headers/loader")
  FetchContent_Declare(
    vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.3.296
  )
  set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
  set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    vulkan_loader
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Loader.git
    GIT_TAG v1.3.296
  )
  set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(UPDATE_DEPS OFF CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(vulkan_headers vulkan_loader)

  if (TARGET Vulkan::Loader)
    add_library(Vulkan::Vulkan ALIAS Vulkan::Loader)
  elseif (TARGET vulkan)
    add_library(Vulkan::Vulkan ALIAS vulkan)
  else()
    message(FATAL_ERROR "Unable to provide Vulkan::Vulkan target")
  endif()
endif()

# Third-party dependencies managed by CMake.
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
)

FetchContent_Declare(
  tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
  GIT_TAG v2.0.0rc13
)
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
set(TINYOBJLOADER_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  vma
  GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
  GIT_TAG v3.1.0
)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

FetchContent_Declare(
  fastgltf
  GIT_REPOSITORY https://github.com/spnda/fastgltf.git
  GIT_TAG v0.9.0
)

FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG v3.13.0
)
set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
set(SIMDJSON_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(SIMDJSON_BUILD_SHARED OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw glm tinyobjloader vma stb fastgltf simdjson)

file(GLOB_RECURSE SOURCES
  ${PROJECT_SOURCE_DIR}/src/*.cpp
  ${PROJECT_SOURCE_DIR}/third_party/*.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})
set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/third_party
  ${stb_SOURCE_DIR}
  ${vma_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  glfw
  Vulkan::Vulkan
  tinyobjloader
  simdjson
)

if (TARGET glm::glm)
  target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
endif()

if (TARGET fastgltf::fastgltf)
  target_link_libraries(${PROJECT_NAME} PRIVATE fastgltf::fastgltf)
else()
  target_include_directories(${PROJECT_NAME} PUBLIC ${fastgltf_SOURCE_DIR}/include)
endif()

if (WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1)
endif()

############## Build SHADERS #######################

file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/shaders/compiled)

find_program(GLSL_VALIDATOR glslangValidator HINTS
  ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
  /usr/bin
  /usr/local/bin
  $ENV{VULKAN_SDK}/Bin/
  $ENV{VULKAN_SDK}/Bin32/
)

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
