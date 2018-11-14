cmake_minimum_required(VERSION 3.2)
     
project(linux_thread_termination)

find_package(Threads)

# Создание целей
add_executable(sleep_interruption sleep_interruption.cpp)
target_link_libraries(sleep_interruption ${CMAKE_THREAD_LIBS_INIT})

add_executable(pthread_cancel pthread_cancel.cpp)
target_link_libraries(pthread_cancel ${CMAKE_THREAD_LIBS_INIT})

add_executable(mask_signal mask_signal.cpp)
target_link_libraries(mask_signal ${CMAKE_THREAD_LIBS_INIT})

# Настройка для всех целей
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wpedantic -Wall -Wextra)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Debug)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Debug)
elseif ((CMAKE_BUILD_TYPE STREQUAL "Release")
         OR (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
         OR (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"))
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Release)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/Release)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

