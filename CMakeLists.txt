cmake_minimum_required(VERSION 3.30)
project(ravel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_PARALLEL_LEVEL 0)

find_package(OpenGL REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/glfw)

add_library(imgui STATIC)
target_sources(imgui PRIVATE
  vendor/imgui/imgui.cpp
  vendor/imgui/imgui_demo.cpp
  vendor/imgui/imgui_draw.cpp
  vendor/imgui/imgui_tables.cpp
  vendor/imgui/imgui_widgets.cpp
  vendor/imgui/backends/imgui_impl_glfw.cpp
  vendor/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC vendor/imgui)
target_link_libraries(imgui PUBLIC 
  glfw
  OpenGL::GL
)

if(WIN32)
    set(PLATFORM_SOURCES src/platform/windows_controller.cpp)
    set(PLATFORM_LIBRARIES psapi)
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_SOURCES src/platform/linux_controller.cpp)
    set(PLATFORM_LIBRARIES)
else()
    message(FATAL_ERROR "Unsupported platform.")
endif()

add_subdirectory(vendor/zydis-bridge)

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/app/application.cpp
  src/core/process.cpp

  ${PLATFORM_SOURCES}

  src/ui/views/processes.cpp
  src/ui/views/memory.cpp
  src/ui/views/disassembly.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE 
  src
  vendor
  vendor/imgui/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  imgui
  zydis-bridge
  ${PLATFORM_LIBRARIES}
  stdc++exp
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
      /W4
      /permissive-
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wpointer-arith
      -Wstrict-overflow=5
      -Wnull-dereference
      -Wformat=2
      -Wcast-align
      -Wstrict-aliasing=2
      # -fno-char8_t
    )
endif()
