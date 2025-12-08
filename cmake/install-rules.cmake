install(
    TARGETS expense_tracquer_exe
    RUNTIME COMPONENT expense_tracquer_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
