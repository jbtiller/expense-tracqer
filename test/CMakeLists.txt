# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(expense_tracquerTests LANGUAGES CXX)

include(FetchContent)

enable_testing()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW    TRUE
  GIT_PROGRESS   TRUE
)

FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# ---- Tests ----

add_executable(
  expense_tracquer_test
  source/expense_tracquer_test.cpp
)

target_include_directories(
  expense_tracquer_test
  PRIVATE SYSTEM ${googletest_SOURCE_DIR}/googletest/include/gtest
)  

target_link_libraries(
  expense_tracquer_test
  PRIVATE expense_tracquer_lib
  GTest::gtest_main
)

# target_compile_features(
#   expense_tracquer_test
#   PRIVATE cxx_std_23
# )

add_test(
  NAME expense_tracquer_test
  COMMAND expense_tracquer_test
)

# ---- End-of-file commands ----

add_folders(Test)
