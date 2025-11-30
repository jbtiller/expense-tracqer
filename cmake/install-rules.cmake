install(
    TARGETS expense-tracquer_exe
    RUNTIME COMPONENT expense-tracquer_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
