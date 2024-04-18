#include "PathComparator.h"
#include "StateCollector.h"
#include <algorithm>
PathComparator::PathComparator(BlockInfoGenerator &preBGen,
                               BlockInfoGenerator &curBGen, PathGenerator &pg1,
                               PathGenerator &pg2, ExecutionEngine &eg1,
                               ExecutionEngine &eg2, BlockMapper &b)
    : preBlockGen(&preBGen), curBlockGen(&curBGen), prevPg(&pg1), afterPg(&pg2),
      prevEg(&eg1), afterEg(&eg2), blockMapper(&b) {}

std::shared_ptr<ExecutionState>
PathComparator::getState(std::vector<clang::CFGBlock *> *path, bool isPrev) {

  std::shared_ptr<ExecutionState> es;
  if (isPrev) {
    es = prevEg->execute(*path);
  } else {
    es = afterEg->execute(*path);
  }

  return es;
}

int PathComparator::getCommonLines(std::vector<clang::CFGBlock *> &p1,
                                   std::vector<clang::CFGBlock *> &p2,
                                   bool isPrev) {

  std::vector<clang::CFGBlock *> *vptr1, *vptr2;

  if (isPrev) {
    vptr1 = &p1;
    vptr2 = &p2;
  } else {
    vptr1 = &p2;
    vptr2 = &p1;
  }
  std::unordered_set<int> preLines;
  std::unordered_set<int> curLines;

  for (auto &b : *vptr1) {
    if (b != preBlockGen->getBegBlock() && b != preBlockGen->getEndBlock()) {
      for (int i = preBlockGen->getBegLine(b); i <= preBlockGen->getEndLine(b);
           i++) {
        preLines.emplace(i);
      }
    }
  }
  for (auto &b : *vptr2) {
    if (b != curBlockGen->getBegBlock() && b != curBlockGen->getEndBlock()) {
      for (int i = curBlockGen->getBegLine(b); i <= curBlockGen->getEndLine(b);
           i++) {
        curLines.emplace(i);
      }
    }
  }

  int cnt = 0;
  for (auto l1 : preLines) {
    auto conL = blockMapper->preLineToCur(l1);
    if (conL == -1) {
      continue;
    } else if (curLines.find(conL) != curLines.end()) {
      cnt++;
    }
  }
  return cnt;
}

int getCommonCondsNum(std::shared_ptr<ExecutionState> pState1,
                      std::shared_ptr<ExecutionState> pState2) {
  int i = 0;
  while (i < pState1->constraints.getAllConstraints().size() &&
         i < pState2->constraints.getAllConstraints().size()) {
    if (pState1->constraints.getAllConstraints()[i]->conStr ==
        pState2->constraints.getAllConstraints()[i]->conStr) {
      i++;
    } else {
      break;
    }
  }

  return i;
}

std::vector<clang::CFGBlock *> &PathComparator::getCorrespondPath(
    std::vector<clang::CFGBlock *> &path,
    std::vector<std::vector<clang::CFGBlock *>> &candidates, bool isPrev) {
  std::shared_ptr<ExecutionState> pState = getState(&path, isPrev);
  std::vector<clang::CFGBlock *> *maxCommPath = nullptr;
  int maxCommonCond = -1;
  int maxCommonLine = -1;
  int maxCurCondNum = -1;
  for (auto b : path) {
    blockMapper->printBlock(b, isPrev);
  }
  for (size_t i = 0; i < candidates.size(); i++) {
    for (auto b : candidates[i])
      blockMapper->printBlock(b, !isPrev);
  }
  size_t candNum = -1;
  for (size_t i = 0; i < candidates.size(); i++) {
    auto &cand = candidates[i];
    std::shared_ptr<ExecutionState> candState = getState(&cand, !isPrev);
    int commonCondsNum = getCommonCondsNum(pState, candState);
    if (commonCondsNum > maxCommonCond) {
      maxCommPath = &cand;
      candNum = i;
      maxCommonCond = commonCondsNum;
      maxCommonLine = getCommonLines(path, cand, isPrev);
      maxCurCondNum = candState->constraints.getAllConstraints().size();
    } else if (commonCondsNum == maxCommonCond) {
      if (candState->constraints.getAllConstraints().size() < maxCurCondNum) {
        maxCurCondNum = candState->constraints.getAllConstraints().size();
        maxCommPath = &cand;
        candNum = i;

      } else {
        int curCommonLineNum = getCommonLines(path, cand, isPrev);
        if (curCommonLineNum > maxCommonLine) {
          maxCommonLine = curCommonLineNum;
          maxCommPath = &cand;
          candNum = i;
        }
      }
    }
  }
  assert(maxCommPath != nullptr);
  return *maxCommPath;
}

Path PathComparator::getCInfo(std::shared_ptr<ExecutionState> state,
                              std::vector<clang::CFGBlock *> &cfgBlocks) {
  Path errPath;
  for (auto cfgBlock : cfgBlocks) {
    for (auto &BlockState : state->blockStates) {
      if (cfgBlock == BlockState.curBlock) {
        for (size_t i = 0; i < BlockState.allBegs.size(); ++i) {
          for (int j = BlockState.allBegs[i].second;
               j <= BlockState.allEnds[i].second; ++j) {
            errPath.isCond = false;
            errPath.rawPath.push_back(j);
          }
        }
      }
    }
  }
  return errPath;
}

Path PathComparator::getCInfo(std::shared_ptr<ExecutionState> state,
                              std::shared_ptr<Constraint> cond) {
  Path errPath;
  for (int i = cond->begLoc.second; i <= cond->endLoc.second; ++i) {
    errPath.isCond = true;
    errPath.rawPath.push_back(i);
  }
  return errPath;
}

Path PathComparator::getCInfo(std::shared_ptr<ExecutionState> state,
                              std::shared_ptr<Identifier> ident,
                              std::vector<int> indexes) {
  Path curPath;
  for (auto index : indexes) {
    if (index >= 0 && index < ident->dataFlowStrs.size()) {

      for (int i = ident->begDataFlows[index].second;
           i <= ident->begDataFlows[index].second; i++) {
        curPath.isCond = false;
        curPath.rawPath.push_back(i);
      }
    }
  }
  return curPath;
}

Path PathComparator::getCInfo(std::shared_ptr<ExecutionState> state,
                              std::shared_ptr<Identifier> ident1,
                              std::shared_ptr<Identifier> ident2) {
  Path errPath;
  if (ident1->qualType.getAsString() != ident2->qualType.getAsString()) {
    for (int i = ident1->begDecl.second; i <= ident1->endDecl.second; ++i) {
      errPath.isCond = false;
      errPath.rawPath.push_back(i);
    }
    return errPath;
  }
  size_t i = 0;
  for (; i < ident1->dataFlowStrs.size() && i < ident2->dataFlowStrs.size();
       ++i) {
    if (ident1->dataFlowStrs[i] != ident2->dataFlowStrs[i]) {
      break;
    }
  }

  if (!(i == ident1->dataFlowStrs.size() && i == ident2->dataFlowStrs.size())) {
    if (i == ident1->dataFlowStrs.size()) {
      std::vector<int> indexes{int(i) - 1, int(i) - 2};
      return getCInfo(state, ident1, indexes);
    } else {
      std::vector<int> indexes{int(i) - 1, int(i)};
      return getCInfo(state, ident1, indexes);
    }
  }
  return errPath;
}



Path PathComparator::getCInfo(std::shared_ptr<ExecutionState> state,
                              int begLineno) {
  Path errPath;
  for (auto &blockState : state->blockStates) {
    for (auto &p : blockState.allBegs) {

      if (p.second >= begLineno) {
        errPath.isCond = true;
        errPath.rawPath.push_back(p.second);
      }
    }
  }
  return errPath;
}

Path PathComparator::getCInfo(std::shared_ptr<ExecutionState> state,
                              std::shared_ptr<Identifier> ident) {
  Path errPath;
  for (size_t i = 0; i < ident->dataFlowStrs.size(); ++i) {
    int begL = ident->begDataFlows[i].second;
    int endL = ident->endDataFlows[i].second;
    for (int j = begL; j <= endL; j++) {
      errPath.isCond = false;
      if (preBlockGen->isModLine(j)) {
        errPath.rawPath.push_back(j);
      }
    }
  }
  return errPath;
}

Path PathComparator::getAddCondInfo(
    std::shared_ptr<ExecutionState> state,
    std::vector<std::shared_ptr<Constraint>> &postCons,
    std::vector<clang::CFGBlock *> &curPath, int i) {
  Path errPath;

  auto postConBlock = postCons[i]->cfgBlock;
  assert(postConBlock != nullptr);
  size_t index =
      std::find(curPath.begin(), curPath.end(), postConBlock) - curPath.begin();
  assert(index != curPath.size());

  int preBlockLastLine = -1;
  int nextBlockBegLine = -1;

  for (int j = postCons[i]->begLoc.second; j >= 0; j--) {
    auto linfo = blockMapper->getLineInfo(j, false);
    if (!linfo) {
      break;
    }
    if (!linfo->isMod) {
      preBlockLastLine = linfo->lineno;
      break;
    }
  }

  for (int j = postCons[i]->endLoc.second;; j++) {
    auto linfo = blockMapper->getLineInfo(j, false);
    if (!linfo) {
      break;
    }
    if (!linfo->isMod) {
      nextBlockBegLine = linfo->lineno;
      break;
    }
  }
  if (preBlockLastLine != -1) {
    int prePreBlockLastLine = blockMapper->curLineToPre(preBlockLastLine);
    if (prePreBlockLastLine != -1) {
      errPath.isCond = true;
      errPath.rawPath.push_back(prePreBlockLastLine);
    }
  }

  if (nextBlockBegLine != -1) {
    int preNextBlockBegLine = blockMapper->curLineToPre(nextBlockBegLine);
    if (preNextBlockBegLine != -1) {
      errPath.isCond = true;
      errPath.rawPath.push_back(preNextBlockBegLine);
    }
  }
  return errPath;
}

std::vector<Path> PathComparator::getDiffLine(
    std::vector<clang::CFGBlock *> &path,
    std::vector<std::vector<clang::CFGBlock *>> &allPath, bool isPrev) {

  std::vector<Path> errPaths;
  auto &mostSuitCand = getCorrespondPath(path, allPath, isPrev);
  for (auto b : path) {
    blockMapper->printBlock(b, isPrev);
  }
  for (auto b : mostSuitCand) {
    blockMapper->printBlock(b, !isPrev);
  }
  std::vector<clang::CFGBlock *> *prePathPtr = nullptr, *curPathPtr = nullptr;
  if (isPrev) {
    prePathPtr = &path;
    curPathPtr = &mostSuitCand;
  } else {
    prePathPtr = &mostSuitCand;
    curPathPtr = &path;
  }

  auto &prePath = *prePathPtr;
  auto &curPath = *curPathPtr;
  auto statePtr1 = getState(&path, isPrev);
  auto statePtr2 = getState(&mostSuitCand, !isPrev);

  auto &constraints1 = statePtr1->constraints.getAllConstraints();
  auto &constraints2 = statePtr2->constraints.getAllConstraints();

  std::shared_ptr<ExecutionState> prePtr, postPtr;
  std::vector<std::shared_ptr<Constraint>> *prevConsPtr, *postConsPtr;
  if (isPrev) {
    prePtr = statePtr1;
    postPtr = statePtr2;
    prevConsPtr = &constraints1;
    postConsPtr = &constraints2;
  } else {
    prePtr = statePtr2;
    postPtr = statePtr1;
    prevConsPtr = &constraints2;
    postConsPtr = &constraints1;
  }

  auto &prevCons = *prevConsPtr;
  auto &postCons = *postConsPtr;
  size_t i = 0;
  for (; i < prevCons.size() && i < postCons.size(); i++) {
    if (!(prevCons[i]->conStr == postCons[i]->conStr)) {
      break;
    }
  }
  
  if (i == prevCons.size() && i < postCons.size()) {
    if (!isPrev) {
      errPaths.push_back(getAddCondInfo(statePtr2, postCons, curPath, i));
      return errPaths;
    }
  } else if (!(i == prevCons.size() && i == postCons.size())) {
    if (!isPrev) {
      errPaths.push_back(getAddCondInfo(statePtr2, postCons, curPath, i));
      return errPaths;
    }
  }

  i = 0;
  for (;
       i < prePtr->globalCallStrs.size() && i < postPtr->globalCallStrs.size();
       i++) {
    if (!(prePtr->globalCallStrs[i] == postPtr->globalCallStrs[i])) {
      break;
    }
  }
  if (i < prePtr->globalCallStrs.size() && i < postPtr->globalCallStrs.size()) {
    Path errPath;
    for (int j = 0; j < prePtr->globalCallBegLoc.size(); j++) {
      for (int k = prePtr->globalCallBegLoc[j].second;
           k <= prePtr->globalCallEndLoc[j].second; k++) {

        errPath.isCond = true;
        errPath.rawPath.push_back(k);
      }
    }
    errPaths.push_back(errPath);
  } else if (i == prePtr->globalCallStrs.size() &&
             i < postPtr->globalCallStrs.size()) {
  }

  for (auto &ident1Pair : prePtr->table.getAllIdentifiers()) {
    auto table_ = postPtr->table.getAllIdentifiers();

    auto ident1Str = ident1Pair.first;
    auto ident1Ptr = ident1Pair.second;
    auto &ident1 = ident1Pair.second;
    if (table_.find(ident1Str) == table_.end()) {
      auto errPath = getCInfo(prePtr, ident1);
      errPath.isCond = false;
      errPaths.push_back(errPath);
      continue;
    }

    auto &ident2Pair = *table_.find(ident1Str);
    auto ident2Str = ident2Pair.first;
    auto &ident2 = ident2Pair.second;
    if (ident1Ptr->isLocal && ident2->isLocal) {
      continue;
    }
    auto errPath = getCInfo(prePtr, ident1, ident2);
    errPath.isCond = false;
    errPaths.push_back(errPath);
  }

  for (auto &ident1Pair : postPtr->table.getAllIdentifiers()) {
    auto table_ = prePtr->table.getAllIdentifiers();
    if (table_.find(ident1Pair.second->name) != table_.end() ||
        ident1Pair.second->isLocal) {
      continue;
    }
    auto ident = ident1Pair.second;
    Path errPath;
    errPath.isCond = false;
    for (int i = 0; i < ident->begDataFlows.size(); i++) {
      for (int j = ident->begDataFlows[i].second;
           j <= ident->endDataFlows[i].second; j++) {
        int preL = blockMapper->curLineToPre(j);
        if (preL != -1) {
          errPath.rawPath.push_back(preL);
        }
      }
    }
    errPaths.push_back(errPath);
  }

  return errPaths;
}

std::vector<Path> PathComparator::getPreDiffLine(
    std::vector<clang::CFGBlock *> &path,
    std::vector<std::vector<clang::CFGBlock *>> &allPath, bool isPrev) {
  std::vector<Path> errPaths;
  auto &mostSuitCand = getCorrespondPath(path, allPath, isPrev);
  for (auto b : path) {
    blockMapper->printBlock(b, isPrev);
  }
  for (auto b : mostSuitCand) {
    blockMapper->printBlock(b, !isPrev);
  }
  std::vector<clang::CFGBlock *> *prePathPtr = nullptr, *curPathPtr = nullptr;
  if (isPrev) {
    prePathPtr = &path;
    curPathPtr = &mostSuitCand;
  } else {
    prePathPtr = &mostSuitCand;
    curPathPtr = &path;
  }

  auto &prePath = *prePathPtr;
  auto &curPath = *curPathPtr;

  auto statePtr1 = getState(&path, isPrev);
  auto statePtr2 = getState(&mostSuitCand, !isPrev);

  auto &constraints1 = statePtr1->constraints.getAllConstraints();
  auto &constraints2 = statePtr2->constraints.getAllConstraints();

  std::shared_ptr<ExecutionState> prePtr, postPtr;
  std::vector<std::shared_ptr<Constraint>> *prevConsPtr, *postConsPtr;
  if (isPrev) {
    prePtr = statePtr1;
    postPtr = statePtr2;
    prevConsPtr = &constraints1;
    postConsPtr = &constraints2;
  } else {
    prePtr = statePtr2;
    postPtr = statePtr1;
    prevConsPtr = &constraints2;
    postConsPtr = &constraints1;
  }

  auto &prevCons = *prevConsPtr;
  auto &postCons = *postConsPtr;
  size_t i = 0;
  for (; i < prevCons.size() && i < postCons.size(); i++) {
    if (!(prevCons[i]->conStr == postCons[i]->conStr)) {
      break;
    }
  }
  
  if (!(i == prevCons.size() && i == postCons.size())) {
    Path errPath;
    for (auto prevCon : prevCons) {
      if (preBlockGen->isModLine(prevCon->begLoc.second)) {
        errPath.isCond = true;
        errPath.rawPath.push_back(prevCon->begLoc.second);
      }
    }
    if (!errPath.rawPath.empty()) {
      errPaths.push_back(errPath);
    }
    if (!errPaths.empty()) {
      return errPaths;
    }
  }
  
  for (auto &ident1Pair : prePtr->table.getAllIdentifiers()) {
    auto table_ = postPtr->table.getAllIdentifiers();

    auto ident1Str = ident1Pair.first;
    auto ident1Ptr = ident1Pair.second;
    
    auto &ident1 = ident1Pair.second;
    if (table_.find(ident1Str) == table_.end()) {
      auto errPath = getCInfo(prePtr, ident1);
      errPath.isCond = false;
      if (!errPath.rawPath.empty()) {
        errPaths.push_back(errPath);
      }
      continue;
    }

    auto &ident2Pair = *table_.find(ident1Str);
    auto ident2Str = ident2Pair.first;
    auto &ident2 = ident2Pair.second;
    if (ident1Ptr->isLocal && ident2->isLocal) {
      continue;
    }
    auto errPath = getCInfo(prePtr, ident1);
    errPath.isCond = false;
    if (!errPath.rawPath.empty()) {
      errPaths.push_back(errPath);
    }
    
  }
  return errPaths;
}

std::vector<Path> PathComparator::getAllInfo() {
  std::vector<Path> errPaths;
  auto &prevExePaths = prevPg->getAllPaths();
  if (prevExePaths.size() > 10) {
    prevExePaths = std::vector<std::vector<clang::CFGBlock *>>(
        prevExePaths.begin(), prevExePaths.begin() + 10);
  }
  for (auto &preP : prevExePaths) {
    auto prePBeg = preP[0];
    auto prePEnd = preP[preP.size() - 1];

    auto curPBeg = blockMapper->getCorrespondBlock(prePBeg, true);

    if (curPBeg == nullptr) {
      continue;
    }
    blockMapper->printBlock(prePBeg, true);
    blockMapper->printBlock(curPBeg, false);
    blockMapper->printBlock(prePEnd, true);
    auto curPEnd = blockMapper->getCorrespondBlock(prePEnd, true);
    auto correspondConPaths =
        afterPg->getPath(curPBeg, curPEnd, preP.size() + 2,
                         blockMapper->getMaxLineno(preP, true));
    std::vector<Path> errPaths_ =
        getPreDiffLine(preP, correspondConPaths, true);
    errPaths.insert(errPaths.end(), errPaths_.begin(), errPaths_.end());
    bool isCond = false;
    for (auto &errPath : errPaths_) {
      if (errPath.isCond) {
        isCond = true;
        break;
      }
    }

    if (isCond) {
      errPaths.clear();
      errPaths.insert(errPaths.end(), errPaths_.begin(), errPaths_.end());
      return errPaths;
    } else {
      errPaths.insert(errPaths.end(), errPaths_.begin(), errPaths_.end());
    }
  }

  if (errPaths.empty()) {
    Path errPath;
    for (auto delLineno : preBlockGen->getModLines()) {
      errPath.isCond = false;
      errPath.rawPath.push_back(delLineno);
    }
    errPaths.push_back(errPath);
  }
  if (!errPaths.empty()) {
    return errPaths;
  }

  auto conExePaths = afterPg->getAllPaths();
  if (conExePaths.size() > 10) {
    conExePaths = std::vector<std::vector<clang::CFGBlock *>>(
        conExePaths.begin(), conExePaths.begin() + 10);
  }
  for (auto &conP : conExePaths) {
    auto conPBeg = conP[0];
    auto conPEnd = conP[conP.size() - 1];

    blockMapper->printBlock(conPBeg, false);
    auto prePBeg = blockMapper->getCorrespondBlock(conPBeg, false);
    if (prePBeg == nullptr) {
      continue;
    }
    auto prePEnd = blockMapper->getCorrespondBlock(conPEnd, false);
    auto correspondConPaths =
        prevPg->getPath(prePBeg, prePEnd, conP.size() + 2,
                        blockMapper->getMaxLineno(conP, false));
    std::vector<Path> errPaths_ = getDiffLine(conP, correspondConPaths, false);
    bool isCond = false;
    for (auto &errPath : errPaths_) {
      if (errPath.isCond) {
        isCond = true;
        break;
      }
    }
    if (isCond) {
      errPaths.clear();
      errPaths.insert(errPaths.end(), errPaths_.begin(), errPaths_.end());
      return errPaths;
    } else {
      errPaths.insert(errPaths.end(), errPaths_.begin(), errPaths_.end());
    }
  }
  return errPaths;
}