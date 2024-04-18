#include "PathGenerator.h"
#include <algorithm>
#include <climits>

PathGenerator::PathGenerator(int sliceNum_,
                             BlockInfoGenerator *blockInfoGenerator_,
                             BlockMapper *blockMapper_, bool isPrev_)
    : sliceNum(sliceNum_), blockInfoGenerator(blockInfoGenerator_),
      blockMapper(blockMapper_), isPrev(isPrev_) {
  genAllPaths();
}

void PathGenerator::genPreds(
    clang::CFGBlock *curBlock, int step,
    std::vector<clang::CFGBlock *> &curPath,
    std::vector<std::vector<clang::CFGBlock *>> &curPaths) {
  blockMapper->printBlock(curBlock, isPrev);
  if (visited.find(curBlock) != visited.end() && step != 0) {
    curPaths.push_back(curPath);
    return;
  }
  visited.emplace(curBlock);

  if (curBlock == blockInfoGenerator->getBegBlock()) {
    curPath.push_back(curBlock);
    curPaths.push_back(curPath);
    curPath.pop_back();
    return;
  }

  if (step >= sliceNum) {
    bool flag = false;
    for (auto iter = curBlock->pred_begin(); iter != curBlock->pred_end();
         iter++) {
      if (*iter != nullptr) {
        if (*iter != nullptr &&
            ((blockInfoGenerator->getBegLine(curBlock) ==
              blockInfoGenerator->getBegLine(*iter)) ||
             (blockInfoGenerator->getBegLine(curBlock) == -1))) {
          flag = true;
          curPath.push_back(curBlock);
          if (curPath.size() < 2 * sliceNum) {
            genPreds(*iter, step, curPath, curPaths);
          } else {
            flag = false;
          }
          curPath.pop_back();
        }
      }
    }
    if (!flag) {
      curPath.push_back(curBlock);
      curPaths.push_back(curPath);
      curPath.pop_back();
    }
    return;
  }

  curPath.push_back(curBlock);
  if (blockInfoGenerator->isMod(curBlock)) {
    step--;
  }

  for (auto iter = curBlock->pred_begin(); iter != curBlock->pred_end();
       iter++) {
    if (*iter != nullptr) {
      // if (blockInfoGenerator->getBegLine(curBlock) ==
      //     blockInfoGenerator->getBegLine(*iter)) {
      //   genPreds(*iter, step, curPath, curPaths);
      // } else {
      genPreds(*iter, step + 1, curPath, curPaths);
      // }
    }
  }
  curPath.pop_back();
}

void PathGenerator::genSuccs(
    clang::CFGBlock *curBlock, int step,
    std::vector<clang::CFGBlock *> &curPath,
    std::vector<std::vector<clang::CFGBlock *>> &curPaths) {
  blockMapper->printBlock(curBlock, isPrev);
  if (visited.find(curBlock) != visited.end() && step != 0) {
    curPaths.push_back(curPath);
    return;
  }
  visited.emplace(curBlock);

  if (curBlock == blockInfoGenerator->getEndBlock()) {
    curPath.push_back(curBlock);
    curPaths.push_back(curPath);
    curPath.pop_back();
    return;
  }
  if (step >= sliceNum) {
    bool flag = false;
    for (auto iter = curBlock->succ_begin(); iter != curBlock->succ_end();
         iter++) {
      if (*iter != nullptr &&
          ((blockInfoGenerator->getBegLine(curBlock) ==
            blockInfoGenerator->getBegLine(*iter)) ||
           (blockInfoGenerator->getBegLine(curBlock) == -1))) {
        flag = true;
        curPath.push_back(curBlock);
        if (curPath.size() < 2 * sliceNum) {
          genSuccs(*iter, step, curPath, curPaths);
        } else {
          flag = false;
        }
        curPath.pop_back();
      }
    }
    if (!flag) {
      curPath.push_back(curBlock);
      curPaths.push_back(curPath);
      curPath.pop_back();
    }
    return;
  }

  if (blockInfoGenerator->isMod(curBlock)) {
    step--;
  }

  curPath.push_back(curBlock);
  for (auto iter = curBlock->succ_begin(); iter != curBlock->succ_end();
       iter++) {
    if (*iter != nullptr) {
      genSuccs(*iter, step + 1, curPath, curPaths);
    }
  }
  curPath.pop_back();
}

int PathGenerator::getSuccCnt(clang::CFGBlock *curBlock) {
  int cnt = 0;
  for (auto iter = curBlock->succ_begin(); iter != curBlock->succ_end();
       iter++) {
    if (*iter != nullptr) {
      cnt++;
    }
  }
  return cnt;
}

void PathGenerator::genSuccs(
    clang::CFGBlock *curBlock, clang::CFGBlock *endBlock, int step,
    std::vector<clang::CFGBlock *> &curPath,
    std::vector<std::vector<clang::CFGBlock *>> &curPaths,
    std::unordered_map<clang::CFGBlock *, int> &cntMap, int maxStep,
    int maxLineno) {
  blockMapper->printBlock(curBlock, isPrev);
  if (curBlock == endBlock || curBlock == blockInfoGenerator->getEndBlock()) {
    
    curPath.push_back(curBlock);
    curPaths.push_back(curPath);
    curPath.pop_back();
    return;
  }
  cntMap[curBlock] = 1;

  int begLine = blockInfoGenerator->getBegLine(curBlock);
  if (begLine != -1 && maxLineno != -1 && begLine > maxLineno) {
    curPaths.push_back(curPath);
    return;
  }

  if (step >= maxStep) {
    bool flag = false;
    for (auto iter = curBlock->succ_begin(); iter != curBlock->succ_end();
         iter++) {
      if (*iter != nullptr) {
        if ((blockInfoGenerator->getBegLine(curBlock) ==
             blockInfoGenerator->getBegLine(*iter)) ||
            (blockInfoGenerator->getBegLine(curBlock) == -1)) {
          flag = true;
          curPath.push_back(curBlock);
          if (curPath.size() < 2 * maxStep) {
            genSuccs(*iter, endBlock, step, curPath, curPaths, cntMap, maxStep,
                     maxLineno);
          } else {
            flag = false;
          }
          curPath.pop_back();
        }
      }
    }
    if (!flag) {
      curPath.push_back(curBlock);
      curPaths.push_back(curPath);
      curPath.pop_back();
    }
    return;
  }

  curPath.push_back(curBlock);

  int succ_cnt = 0;
  for (auto iter = curBlock->succ_begin(); iter != curBlock->succ_end();
       iter++) {
    succ_cnt++;
  }

  if (getSuccCnt(curBlock) == 0) {
    curPaths.push_back(curPath);
    curPath.pop_back();
    return;
  }
  for (auto iter = curBlock->succ_begin(); iter != curBlock->succ_end();
       iter++) {
    if (*iter != nullptr) {

      genSuccs(*iter, endBlock, step + 1, curPath, curPaths, cntMap, maxStep,
               maxLineno);
    }
  }
  curPath.pop_back();
}

void PathGenerator::genAllPaths() {
  for (auto cblock : blockInfoGenerator->getChangedBlocks()) {
    if (visited.find(cblock) != visited.end()) {
      continue;
    }
    std::vector<clang::CFGBlock *> prevCurPath;
    std::vector<std::vector<clang::CFGBlock *>> prevPaths;
    genPreds(cblock, 0, prevCurPath, prevPaths);
    std::vector<clang::CFGBlock *> conCurPath;
    std::vector<std::vector<clang::CFGBlock *>> curPaths;
    for (auto iter = cblock->succ_begin(); iter != cblock->succ_end(); iter++) {
      if (*iter != nullptr) {
        genSuccs(*iter, 0, conCurPath, curPaths);
      }
    }
    
    if (curPaths.empty()) {
      for (auto &p : prevPaths) {
        std::reverse(p.begin(), p.end());
        allPaths.push_back(p);
      }
      return;
    }

    if (prevPaths.empty()) {
      for (auto &p : curPaths) {
        p.insert(p.begin(), cblock);
        allPaths.push_back(p);
      }
      return;
    }

    for (auto &p1 : prevPaths) {
      std::reverse(p1.begin(), p1.end());
      for (auto &p2 : curPaths) {
        std::vector<clang::CFGBlock *> finalPath;

        finalPath.insert(finalPath.end(), p1.begin(), p1.end());
        finalPath.insert(finalPath.end(), p2.begin(), p2.end());
        allPaths.push_back(finalPath);
      }
    }
  }
}

std::vector<std::vector<clang::CFGBlock *>>
PathGenerator::getPath(clang::CFGBlock *beg, clang::CFGBlock *end, int maxStep,
                       int maxLineno) {
  std::vector<std::vector<clang::CFGBlock *>> retPaths;

  std::vector<clang::CFGBlock *> curPath;
  std::vector<std::vector<clang::CFGBlock *>> curPaths;
  std::unordered_map<clang::CFGBlock *, int> cntMap;
  genSuccs(beg, end, 0, curPath, curPaths, cntMap, maxStep, maxLineno);
  return curPaths;
}

std::vector<std::vector<clang::CFGBlock *>> &PathGenerator::getAllPaths() {
  return allPaths;
}