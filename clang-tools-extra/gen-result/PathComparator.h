#ifndef MY_PATH_COMPARATOR
#define MY_PATH_COMPARATOR
#include "BlockInfoGenerator.h"
#include "BlockMapper.h"
#include "PathGenerator.h"
#include "StateCollector.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct Path {
  bool isCond = false;
  std::vector<int> rawPath;
};

class PathComparator {
private:
  BlockInfoGenerator *preBlockGen;
  BlockInfoGenerator *curBlockGen;
  PathGenerator *prevPg;
  PathGenerator *afterPg;
  BlockMapper *blockMapper;

  ExecutionEngine *prevEg;
  ExecutionEngine *afterEg;
  std::unordered_map<std::vector<clang::CFGBlock *> *,
                     std::shared_ptr<ExecutionState>>
      stateMap;
  int getCommonLines(std::vector<clang::CFGBlock *> &p1,
                     std::vector<clang::CFGBlock *> &p2, bool isPrev);

public:
  PathComparator(BlockInfoGenerator &preBGen, BlockInfoGenerator &curBGen,
                 PathGenerator &pg1, PathGenerator &pg2, ExecutionEngine &eg1,
                 ExecutionEngine &eg2, BlockMapper &b);

  std::vector<Path>
  getDiffLine(std::vector<clang::CFGBlock *> &path,
              std::vector<std::vector<clang::CFGBlock *>> &allPath,
              bool isPrev);

  std::vector<Path>
  getPreDiffLine(std::vector<clang::CFGBlock *> &path,
              std::vector<std::vector<clang::CFGBlock *>> &allPath,
              bool isPrev);

  std::vector<clang::CFGBlock *> &
  getCorrespondPath(std::vector<clang::CFGBlock *> &path,
                    std::vector<std::vector<clang::CFGBlock *>> &candidates,
                    bool isPrev);

  std::shared_ptr<ExecutionState> getState(std::vector<clang::CFGBlock *> *path,
                                           bool isPrev);

  Path getCInfo(std::shared_ptr<ExecutionState> state,
                std::vector<clang::CFGBlock *> &cfgBlocks);

  Path getCInfo(std::shared_ptr<ExecutionState> state,
                std::shared_ptr<Constraint> cond);

  // std::vector<CInfo> getCInfo(std::shared_ptr<ExecutionState> state,
  //                             clang::CFGBlock *block);

  Path getCInfo(std::shared_ptr<ExecutionState> state,
                std::shared_ptr<Identifier> ident);

  Path getCInfo(std::shared_ptr<ExecutionState> state,
                std::shared_ptr<Identifier> ident1,
                std::shared_ptr<Identifier> ident2);

  // std::vector<CInfo> getCInfo(std::shared_ptr<ExecutionState> state,
  //                             int begLineno, int endLineno);

  Path getCInfo(std::shared_ptr<ExecutionState> state, int begLineno);

  Path getCInfo(std::shared_ptr<ExecutionState> state,
                std::shared_ptr<Identifier> ident, std::vector<int> indexes);

  Path getAddCondInfo(std::shared_ptr<ExecutionState> state,
                      std::vector<std::shared_ptr<Constraint>> &postCons,
                      std::vector<clang::CFGBlock *> &curPath, int i);

  std::vector<Path> getAllInfo();
};
#endif