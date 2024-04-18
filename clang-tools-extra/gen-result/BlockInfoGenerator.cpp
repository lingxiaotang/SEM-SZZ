#include "BlockInfoGenerator.h"
#include "StateCollector.h"
#include <algorithm>
#include <utility>
std::pair<int, std::string> getLocInfo(clang::SourceLocation *sl,
                                       clang::SourceManager *sm) {
  clang::PresumedLoc pLoc = sm->getPresumedLoc(*sl);
  return std::make_pair<int, std::string>(pLoc.getLine(), pLoc.getFilename());
}

void BlockInfoGenerator::initializeInfo() {
  for (auto iter = cfgPtr->nodes_begin(); iter != cfgPtr->nodes_end(); iter++) {
    auto cfgBlock = *iter;
    if (cfgBlock == &cfgPtr->getEntry()) {
      this->begBlock = cfgBlock;
      break;
    }
  }

  for (auto iter = cfgPtr->nodes_begin(); iter != cfgPtr->nodes_end(); iter++) {
    auto cfgBlock = *iter;
    if (cfgBlock == &cfgPtr->getExit()) {
      this->endBlock = cfgBlock;
      break;
    }
  }

  for (auto iter = cfgPtr->nodes_begin(); iter != cfgPtr->nodes_end(); iter++) {
    auto cfgBlock = *iter;
    if (cfgBlock == &cfgPtr->getEntry()) {
      allBlocks.push_back(std::make_pair<>(cfgBlock, 0));
    } else if (cfgBlock == &cfgPtr->getExit()) {
      allBlocks.push_back(std::make_pair<>(cfgBlock, INT_MAX));
    } else {
      int lineno = getBegLine(cfgBlock);
      if (lineno != -1) {
        allBlocks.push_back(std::make_pair<>(cfgBlock, lineno));
      }
    }
  }

  std::sort(allBlocks.begin(), allBlocks.end(),
            [](auto &p1, auto &p2) { return p1.second < p2.second; });

  for (auto lineno : this->clines) {
    auto cfgBlocks = findBlock(lineno);
    for (auto cfgBlock : cfgBlocks) {
      if ((cfgBlock != nullptr) &&
          (std::find(changedBlocks.begin(), changedBlocks.end(), cfgBlock) ==
           changedBlocks.end())) {
        changedBlocks.push_back(cfgBlock);
      }
    }
  }
}

BlockInfoGenerator::BlockInfoGenerator(std::vector<int> &clines_,
                                       clang::CFG *cfgPtr_,
                                       clang::SourceManager *sm_,
                                       clang::ASTContext *astContext_)
    : clines{clines_}, cfgPtr{cfgPtr_}, sm{sm_}, astContext(astContext_) {
  std::sort(clines.begin(), clines.end());
  initializeInfo();
}

bool BlockInfoGenerator::isModLine(int lineno) {
  return std::find(clines.begin(), clines.end(), lineno) != clines.end();
}

int BlockInfoGenerator::getBegLine(clang::CFGBlock *b) {
  if (clang::Stmt *label = const_cast<clang::Stmt *>(b->getLabel())) {
    auto loc = label->getBeginLoc();
    auto lineInfo = getLocInfo(&loc, this->sm);
    return lineInfo.first;
  }
  if (b == this->begBlock) {
    return 0;
  }
  if (b == this->endBlock) {
    return INT_MAX;
  }
  for (auto iter = b->begin(); iter != b->end(); iter++) {
    auto &cfgElement = *iter;
    if (cfgElement.getKind() == clang::CFGElement::Kind::Statement) {
      clang::CFGStmt CS = cfgElement.castAs<clang::CFGStmt>();
      const clang::Stmt *stmt = CS.getStmt();
      clang::SourceLocation l = stmt->getBeginLoc();
      auto lineInfo = getLocInfo(&l, this->sm);
      return lineInfo.first;
    }
  }
  return -1;
}

std::vector<int> BlockInfoGenerator::getStmtLines(clang::CFGBlock *b) {
  std::vector<int> stmtLines;
  if (clang::Stmt *label = const_cast<clang::Stmt *>(b->getLabel())) {
    auto loc = label->getBeginLoc();
    auto lineInfo = getLocInfo(&loc, this->sm);
    stmtLines.push_back(lineInfo.first);
  }
  for (auto iter = b->begin(); iter != b->end(); iter++) {
    auto &cfgElement = *iter;
    if (cfgElement.getKind() == clang::CFGElement::Kind::Statement) {
      clang::CFGStmt CS = cfgElement.castAs<clang::CFGStmt>();
      const clang::Stmt *stmt = CS.getStmt();
      clang::SourceLocation begl = stmt->getBeginLoc();
      auto begLineInfo = getLocInfo(&begl, this->sm);

      clang::SourceLocation endl = stmt->getEndLoc();
      auto endLineInfo = getLocInfo(&endl, this->sm);
      for (int i = begLineInfo.first; i <= endLineInfo.first; i++) {
        stmtLines.push_back(i);
      }
    }
  }

  if (b->getTerminator().isValid()) {
    auto condStmt = b->getTerminatorCondition();
    if (condStmt) {
      auto begL = condStmt->getBeginLoc();
      auto begLineinfo = getLocInfo(&begL, this->sm);
      auto endL = condStmt->getEndLoc();
      auto endLineinfo = getLocInfo(&endL, this->sm);
      for (int i = begLineinfo.first; i <= endLineinfo.first; i++) {
        stmtLines.push_back(i);
      }
      return stmtLines;
    }
    auto stmt = b->getTerminatorStmt();
    if (stmt) {
      auto begL = stmt->getBeginLoc();
      auto begLineinfo = getLocInfo(&begL, this->sm);
      auto endL = stmt->getEndLoc();
      auto endLineinfo = getLocInfo(&endL, this->sm);
      for (int i = begLineinfo.first; i <= begLineinfo.first; i++) {
        stmtLines.push_back(i);
      }
    }
  }
  return stmtLines;
}

int BlockInfoGenerator::getEndLine(clang::CFGBlock *b) {
  if (b == this->begBlock) {
    return 0;
  }
  if (b == this->endBlock) {
    return INT_MAX;
  }

  if (b->getTerminator().isValid()) {
    
    auto condStmt = b->getTerminatorCondition();
    if (condStmt) {
      auto l = condStmt->getEndLoc();
      auto lineinfo = getLocInfo(&l, this->sm);
      return lineinfo.first;
    }
    auto stmt = b->getTerminatorStmt();
    if (stmt) {
      auto l = stmt->getBeginLoc();
      auto lineinfo = getLocInfo(&l, this->sm);
      return lineinfo.first;
    }
  }

  for (auto iter = b->rbegin(); iter != b->rend(); iter++) {
    auto &cfgElement = *iter;
    if (cfgElement.getKind() == clang::CFGElement::Kind::Statement) {
      clang::CFGStmt CS = cfgElement.castAs<clang::CFGStmt>();
      const clang::Stmt *stmt = CS.getStmt();
      clang::SourceLocation l = stmt->getEndLoc();
      auto lineInfo = getLocInfo(&l, this->sm);
      return lineInfo.first;
    }
  }

  if (clang::Stmt *label = const_cast<clang::Stmt *>(b->getLabel())) {
    auto loc = label->getEndLoc();
    auto lineInfo = getLocInfo(&loc, this->sm);
    return lineInfo.first;
  }
  return -1;
}

bool BlockInfoGenerator::isMod(clang::CFGBlock *cfgBlock) {
  return std::find(changedBlocks.begin(), changedBlocks.end(), cfgBlock) !=
         changedBlocks.end();
}

clang::CFGBlock *BlockInfoGenerator::getBegBlock() { return this->begBlock; }

clang::CFGBlock *BlockInfoGenerator::getEndBlock() { return this->endBlock; }

std::vector<clang::CFGBlock *> BlockInfoGenerator::findBlock(int lineno) {
  std::vector<clang::CFGBlock *> ret;
  auto iter = std::upper_bound(
      allBlocks.begin(), allBlocks.end(),
      std::pair<clang::CFGBlock *, int>(nullptr, lineno),
      [](const auto &p1, const auto &p2) { return p1.second < p2.second; });
  assert((iter != allBlocks.begin()) && (iter != allBlocks.end()));
  iter--;
  if (iter == allBlocks.begin()) {
    return ret;
  }
  while (1) {
    auto toCheck = (*iter).first;
    if (getEndLine(toCheck) >= lineno) {
      ret.push_back((*iter).first);
      iter--;
    } else {
      break;
    }
  }

  return ret;
}