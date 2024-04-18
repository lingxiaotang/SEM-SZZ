#ifndef MY_PATHGEN_H
#define MY_PATHGEN_H
#include "BlockInfoGenerator.h"
#include "BlockMapper.h"
#include "clang/Analysis/CFG.h"
#include "clang/Basic/SourceManager.h"
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>
#define TRIMSIZE 1

class PathGenerator {
private:
  int sliceNum;
  BlockInfoGenerator *blockInfoGenerator;
  BlockMapper *blockMapper;
  std::unordered_set<clang::CFGBlock *> visited;
  bool isPrev;
  std::vector<std::vector<clang::CFGBlock *>> allPaths;
  int getSuccCnt(clang::CFGBlock *curBlock);
public:
  void genPreds(clang::CFGBlock *curBlock, int step,
                std::vector<clang::CFGBlock *> &curPath,
                std::vector<std::vector<clang::CFGBlock *>> &allPaths);

  void genSuccs(clang::CFGBlock *curBlock, int step,
                std::vector<clang::CFGBlock *> &curPath,
                std::vector<std::vector<clang::CFGBlock *>> &allPaths);

  void genSuccs(clang::CFGBlock *curBlock, clang::CFGBlock *endBlock, int step,
                std::vector<clang::CFGBlock *> &curPath,
                std::vector<std::vector<clang::CFGBlock *>> &allPaths,
                std::unordered_map<clang::CFGBlock *,int> &cntMap, int maxStep,
                int maxLineno);

  void genAllPaths();
  PathGenerator(int sliceNum_, BlockInfoGenerator *blockInfoGenerator_,
                BlockMapper *blockMapper_, bool isPrev_);

  std::vector<std::vector<clang::CFGBlock *>> &getAllPaths();

  std::vector<std::vector<clang::CFGBlock *>> getPath(clang::CFGBlock *beg,
                                                      clang::CFGBlock *end,
                                                      int maxStep,
                                                      int maxLineno);
};

#endif