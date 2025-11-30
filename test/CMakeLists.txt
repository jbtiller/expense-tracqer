# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(expense-tracquerTests LANGUAGES CXX)

# ---- Tests ----

add_executable(expense-tracquer_test source/expense-tracquer_test.cpp)
# target_link_libraries(expense-tracquer_test PRIVATE expense-tracquer_lib)
target_compile_features(expense-tracquer_test PRIVATE cxx_std_20)

add_test(NAME expense-tracquer_test COMMAND expense-tracquer_test)

# ---- End-of-file commands ----

add_folders(Test)
