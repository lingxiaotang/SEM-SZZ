set(LLVM_LINK_COMPONENTS support)
set(CMAKE_CXX_FLAGS "-g" )


add_clang_executable(aszz-bin
  aszz.cpp
)
target_link_libraries(aszz-bin
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  # blockMapper
)
