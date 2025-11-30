cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    expense-tracquer
    VERSION 0.1.0
    DESCRIPTION "GUI for entering and exploring my expenses"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

set(CMAKE_FIND_DEBUG_MODE 1)

find_package(Qt6 REQUIRED COMPONENTS
    Widgets
)

qt_standard_project_setup()

# ---- Declare library ----

add_library(
    expense-tracquer_lib OBJECT
    source/expense_lib.cpp
)

target_include_directories(
    expense-tracquer_lib ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(expense-tracquer_lib PUBLIC cxx_std_20)

# ---- Declare executable ----

qt_add_executable(
  expense-tracquer_exe
    source/main.cpp
    source/expense_tracquer.cpp
)
add_executable(
    expense-tracquer::exe
    ALIAS expense-tracquer_exe
)

set_property(TARGET expense-tracquer_exe
    PROPERTY OUTPUT_NAME expense-tracquer
)

target_include_directories(
    expense-tracquer_exe ${warning_guard}
    PUBLIC
    "\$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(expense-tracquer_exe
    PRIVATE cxx_std_20
)

target_link_libraries(expense-tracquer_exe
    PRIVATE expense-tracquer_lib
    Qt6::Widgets
)

# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT expense-tracquer_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of expense-tracquer"
  )
endif()

include(cmake/dev-mode.cmake)
