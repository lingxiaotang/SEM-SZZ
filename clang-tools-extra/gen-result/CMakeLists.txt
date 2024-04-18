set(LLVM_LINK_COMPONENTS support)
set(CMAKE_CXX_FLAGS "-g" )


add_clang_executable(gen-result
  GenResult.cpp
  BlockInfoGenerator.cpp
  BlockMapper.cpp
  constraint.cpp
  identifierTable.cpp
  PathComparator.cpp
  PathGenerator.cpp
  StateCollector.cpp
)
target_link_libraries(gen-result
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  # blockMapper
)
# target_link_libraries(
#   PUBLIC
#   blockMapper
# )