# console

cmake_minimum_required(VERSION 3.10)

set(console_files
    console.cpp
    console.hpp
    shell.cpp
    shell.hpp)

set(shell_files
    console.cpp
    console.hpp
    shell.cpp
    shell.hpp
    main.cpp)

# trivial and stupid checking but may work
if(SYSTEM_OS_LINUX OR SYSTEM_OS_WINDOWS)
  message(STATUS "Build console as subproject")
  include_directories(../framework)
  add_library(console STATIC ${console_files})
  link_libraries(framework)
else()
  project(app CXX)

  if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
      message(STATUS "OS is ${CMAKE_SYSTEM_NAME}")
      set(SYSTEM_OS_LINUX 1)
      set(SYSTEM_OS_WINDOWS 0)
  elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
      message(STATUS "OS is ${CMAKE_SYSTEM_NAME}")
      set(SYSTEM_OS_LINUX 0)
      set(SYSTEM_OS_WINDOWS 1)
  endif()
  add_definitions(-DSYSTEM_OS_LINUX=${SYSTEM_OS_LINUX})
  add_definitions(-DSYSTEM_OS_WINDOWS=${SYSTEM_OS_WINDOWS})
  message(STATUS "Add definition: -DSYSTEM_OS_LINUX=${SYSTEM_OS_LINUX}")
  message(STATUS "Add definition: -DSYSTEM_OS_WINDOWS=${SYSTEM_OS_WINDOWS}")

  add_definitions(-DSTANDALONE_PROJECT_CONSOLE=1)
  message(STATUS "Add definition STANDALONE_PROJECT_CONSOLE=1")
  add_executable(app ${shell_files})
  set_property(TARGET app PROPERTY CXX_STANDARD 17)
endif()
