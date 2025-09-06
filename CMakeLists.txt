# taken from Brendan vulkan tutorial
# https://github.com/blurrypiano/littleVulkanEngine/blob/main/CMakeLists.txt

include(./.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV) # include a local environment file for custom paths
message(STATUS "Local .env.cmake: ${LOCAL_ENV}")

cmake_minimum_required(VERSION 3.11.0)

set(NAME VulkanApp) # set the application name

message(STATUS "using ${CMAKE_GENERATOR}") # Display the CMake generator being used (e.g., "Visual Studio 16 2019", "MinGW Makefiles")
if (CMAKE_GENERATOR STREQUAL "MinGW Makefiles") # Check if the generator is for MinGW Makefiles
    if (NOT MINGW_PATH) # If using MinGW, ensure the path is set
      message(FATAL_ERROR "MINGW_PATH not set in .env.cmake") # Halt the build if MINGW_PATH is not set
    endif()
    set(USE_MINGW "True") # Set a flag to indicate MinGW is being used
    set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/gcc.exe) 
    set(CMAKE_CXX_COMPILER  ${MINGW_PATH}/bin/g++.exe) # Explicitly set the C++ compiler for MinGW
endif()

project(${NAME} VERSION 0.1.0) # build version

# 1. Set VULKAN_SDK_PATH in .env.cmake to target specific vulkan version
if (DEFINED VULKAN_SDK_PATH)
  set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/Include") # 1.1 Make sure this include path is correct
  set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/Lib") # 1.2 Make sure lib path is correct
  set(Vulkan_FOUND "True")
else()
  # If not using a custom path, find the system-installed Vulkan SDK.
  find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
  message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
endif()
if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
else()
	message(STATUS "Using vulkan lib at: ${Vulkan_LIBRARIES}")
endif()


# 2. Set GLFW_PATH in .env.cmake to target specific glfw
if (DEFINED GLFW_PATH)
  message(STATUS "Using GLFW path specified in .env")
  set(GLFW_INCLUDE_DIRS "${GLFW_PATH}/include")
  if (MSVC)
    set(GLFW_LIB "${GLFW_PATH}/lib-vc2022") # 2.1 Update lib-vc2022 to use same version as your visual studio
  elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    message(STATUS "USING MINGW")
    set(GLFW_LIB "${GLFW_PATH}/lib-mingw-w64") # 2.1 make sure matches glfw mingw subdirectory
  endif()
else()
  # If no custom path, use CMake's find_package to locate GLFW 3.3 or newer.
  find_package(glfw3 3.3 REQUIRED)
  set(GLFW_LIB glfw) # Use the target name provided by find_package for linking.
  message(STATUS "Found GLFW")
endif()
if (NOT GLFW_LIB) # After attempting to find GLFW, check if it was successful.
	message(FATAL_ERROR "Could not find glfw library!") # Halt the build if GLFW is not found.
else()
	message(STATUS "Using glfw lib at: ${GLFW_LIB}")
endif()

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/src/*.cpp) # Find all .cpp files in the src/ directory and its subdirectories

add_executable(${PROJECT_NAME} ${SOURCES}) # Create an executable target from the collected source files

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# Set the C++ standard for the target
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

if (WIN32)
  message(STATUS "CREATING BUILD FOR WINDOWS")

  if (USE_MINGW) # If using MinGW on Windows, add its specific paths
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/include
    )
    target_link_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/lib
    )
  endif()

  # Add all necessary include directories
  target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${TINYOBJ_PATH}
    ${STB_IMAGE_PATH}
    ${GLFW_INCLUDE_DIRS}
    ${GLM_PATH}
    )

  # Add all necessary library directories for the linker
  target_link_directories(${PROJECT_NAME} PUBLIC
    ${Vulkan_LIBRARIES}
    ${GLFW_LIB}
  )

  target_link_libraries(${PROJECT_NAME} glfw3 vulkan-1) # Link the executable against the GLFW and Vulkan libraries
elseif (UNIX)
    message(STATUS "CREATING BUILD FOR UNIX")
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${PROJECT_SOURCE_DIR}/src
      ${TINYOBJ_PATH}
    )
    target_link_libraries(${PROJECT_NAME} glfw ${Vulkan_LIBRARIES})
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
  ${VULKAN_SDK_PATH}/Bin
  ${VULKAN_SDK_PATH}/Bin32
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
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
)