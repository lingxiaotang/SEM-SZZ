set(LLVM_LINK_COMPONENTS support)
set(CMAKE_CXX_FLAGS "-g")
add_clang_executable(gen-line-result
  genLineResult.cpp
  )
target_link_libraries(gen-line-result
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  )