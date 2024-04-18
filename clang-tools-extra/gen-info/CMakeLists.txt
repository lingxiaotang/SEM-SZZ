set(LLVM_LINK_COMPONENTS support)
set(CMAKE_CXX_FLAGS "-g")
add_clang_executable(gen-info
  GenInfo.cpp
  )
target_link_libraries(gen-info
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  )