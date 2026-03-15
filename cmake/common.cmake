if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(PROJECT_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
set(EXE_NAME ${PROJECT_NAME})

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the source code and call cmake from there. Thank you.")
endif()

add_definitions(-D_USE_MATH_DEFINES)

include_directories(
  "${PROJECT_PATH}/include"
)

set_target_properties(${EXE_NAME}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
