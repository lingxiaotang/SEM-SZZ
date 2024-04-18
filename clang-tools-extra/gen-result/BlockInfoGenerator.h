#ifndef MY_BLOCK_INFO_GENERATOR
#define MY_BLOCK_INFO_GENERATOR
#include "clang/Analysis/CFG.h"
#include "clang/Basic/SourceManager.h"
#include <unordered_set>
std::pair<int, std::string> getLocInfo(clang::SourceLocation *sl,
                                       clang::SourceManager *sm);

class BlockInfoGenerator {
  clang::CFG *cfgPtr;
  clang::SourceManager *sm;
  clang::CFGBlock *begBlock = nullptr;
  clang::CFGBlock *endBlock = nullptr;
  clang::ASTContext *astContext;
  std::vector<clang::CFGBlock *> changedBlocks;
  std::vector<std::pair<clang::CFGBlock *, int>> allBlocks;
  std::vector<int> clines;
  void initializeInfo();

public:
  BlockInfoGenerator(std::vector<int> &clines_, clang::CFG *cfgPtr_,
                     clang::SourceManager *sm_, clang::ASTContext *astContext_);
  int getBegLine(clang::CFGBlock *b);
  int getEndLine(clang::CFGBlock *b);
  bool isMod(clang::CFGBlock *cfgBlock);
  clang::CFGBlock *getBegBlock();
  clang::CFGBlock *getEndBlock();
  std::vector<clang::CFGBlock *> findBlock(int lineno);
  std::vector<int> &getModLines() { return clines; }
  std::vector<clang::CFGBlock *> &getChangedBlocks() { return changedBlocks; }
  std::vector<std::pair<clang::CFGBlock *, int>> &getAllBlocks() {
    return allBlocks;
  }
  clang::ASTContext *getAstContext() { return astContext; }
  std::vector<int> getStmtLines(clang::CFGBlock *b);
  bool isModLine(int lineno);
};

#endif