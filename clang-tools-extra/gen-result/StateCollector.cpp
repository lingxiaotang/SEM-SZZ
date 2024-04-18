#include "StateCollector.h"
#include "clang/AST/Decl.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceLocation.h"
#define DEFAULTVAL INT_MIN

size_t ExecutionState::getBlockIndex(clang::CFGBlock *cfg) {
  if (cfg == nullptr) {
    return 0;
  }

  for (size_t i = 0; i < blockStates.size(); ++i) {
    if (blockStates[i].curBlock == cfg) {
      return i;
    }
  }

  assert(0);
  return -1;
}

BlockState &ExecutionState::getTopBlockState() {
  assert(this->blockStates.size() > 0);
  return blockStates[blockStates.size() - 1];
}

ExecutionEngine::ExecutionEngine(
    clang::ASTContext *c, clang::LangOptions *l, clang::SourceManager *s,
    BlockMapper *bm, BlockInfoGenerator *b, bool isPrev,
    std::unordered_map<int, std::vector<std::string>> &lineTokMap)
    : context(c), langOpts(l), sourceManager(s), bm(bm), binfoGen(b),
      isPrev(isPrev), lineTokMap(&lineTokMap) {}

VarDeclVisitor::VarDeclVisitor(clang::ASTContext *c) : context(c) {}

bool VarDeclVisitor::VisitDeclRefExpr(clang::DeclRefExpr *D) {
  if (D->getDecl()) {
    decls.push_back(D);
  }
  return true;
}

std::vector<clang::DeclRefExpr *> &VarDeclVisitor::getDeclRefs() {
  return decls;
}

std::pair<std::string, int>
ExecutionEngine::getLineInfo(clang::SourceManager *sm,
                             clang::SourceLocation sl) {
  clang::PresumedLoc pLoc = sm->getPresumedLoc(sl);
  assert(pLoc.isValid());
  return std::make_pair<std::string, int>("", pLoc.getLine());
}

std::string ExecutionEngine::getStmtAsString(const clang::Stmt *stmt,
                                             clang::ASTContext *context) {
  clang::PrintingPolicy printPolicy(context->getLangOpts());
  std::string exprString;
  llvm::raw_string_ostream stream(exprString);
  stmt->printPretty(stream, nullptr, printPolicy);
  stream.flush();
  return exprString;
}

bool ExecutionEngine::isPureLocal(clang::DeclRefExpr *D,
                                  clang::SourceManager *sm) {
  if (D->getDecl()->getKind() == clang::ValueDecl::ParmVar) {
    if (D->getType() == context->DependentTy) {
      return false;
    }
    if (clang::BuiltinType::classof(D->getType().getTypePtr())) {
      return true;
    }
    return false;
  }

  auto begLoc = D->getBeginLoc();
  auto endLoc = D->getEndLoc();
  int begLine = ExecutionEngine::getLineInfo(sm, begLoc).second;
  int endLine = ExecutionEngine::getLineInfo(sm, endLoc).second;

  auto varLoc = D->getBeginLoc();
  int varLine = ExecutionEngine::getLineInfo(sm, varLoc).second;

  if (varLine < begLine || varLine > endLine) {
    return true;
  }

  return false;
}

void ExecutionEngine::dealVarDecl(const clang::VarDecl *varDecl,
                                  std::shared_ptr<ExecutionState> es,
                                  bool addVarLoc) {
  auto varDeclStmt = static_cast<const clang::VarDecl *>(varDecl);
  std::shared_ptr<Identifier> iPtr = std::make_shared<Identifier>();
  std::unordered_set<std::string> visited;
  iPtr->qualType = varDeclStmt->getType();
  iPtr->name = varDeclStmt->getNameAsString();
  visited.emplace(iPtr->name);
  if (addVarLoc) {
    iPtr->begDecl = getLineInfo(this->sourceManager, varDecl->getBeginLoc());
    iPtr->endDecl = getLineInfo(this->sourceManager, varDecl->getEndLoc());
  }
  if (addVarLoc && varDeclStmt->getInit() != nullptr) {
    auto initExpr = varDeclStmt->getInit();
    iPtr->dataFlows.push_back(const_cast<clang::Expr *>(initExpr));
    iPtr->dataFlowStrs.push_back(getStmtAsString(initExpr, context));
    iPtr->begDataFlows.push_back(
        getLineInfo(this->sourceManager, initExpr->getBeginLoc()));
    iPtr->endDataFlows.push_back(
        getLineInfo(this->sourceManager, initExpr->getEndLoc()));

    VarDeclVisitor v{this->context};
    v.TraverseStmt(const_cast<clang::Expr *>(initExpr));
    for (auto declRef : v.getDeclRefs()) {
      visited.emplace(declRef->getNameInfo().getAsString());
      iPtr->isLocal = iPtr->isLocal && ExecutionEngine::isPureLocal(
                                           declRef, this->sourceManager);
    }
  }
  es->table.addIdentifier(iPtr);
  if (varDeclStmt->getInit() == nullptr) {
    return;
  }
  int begLine = getLineInfo(this->sourceManager, varDecl->getBeginLoc()).second;
  int endLine = getLineInfo(this->sourceManager, varDecl->getBeginLoc()).second;
  for (auto identPair : es->table.getAllIdentifiers()) {
    auto name = identPair.first;
    if (visited.find(name) == visited.end()) {
      for (int i = begLine; i <= endLine; i++) {
        auto &lineTokens = (*lineTokMap)[i];
        if (std::find(lineTokens.begin(), lineTokens.end(), name) !=
            lineTokens.end()) {
          auto ident = es->table.getIdentifer(name);
          ident->dataFlows.push_back(
              const_cast<clang::Expr *>(varDeclStmt->getInit()));
          ident->dataFlowStrs.push_back(
              getStmtAsString(varDeclStmt->getInit(), context));
          ident->begDataFlows.push_back(
              getLineInfo(this->sourceManager, varDecl->getBeginLoc()));
          ident->endDataFlows.push_back(
              getLineInfo(this->sourceManager, varDecl->getBeginLoc()));
          ident->isLocal = false;
          break;
        }
      }
    }
  }
}

void ExecutionEngine::dealFuncCall(const clang::CallExpr *callExpr,
                                   std::shared_ptr<ExecutionState> es) {

  VarDeclVisitor v{context};
  v.TraverseStmt(const_cast<clang::CallExpr *>(callExpr));
  std::unordered_set<std::string> visited;
  for (auto declRef : v.getDeclRefs()) {
    std::string varName = declRef->getNameInfo().getAsString();
    if (visited.find(varName) != visited.end()) {
      continue;
    }
    visited.emplace(varName);
    if (!es->table.getIdentifer(varName)) {
      if (clang::VarDecl::classof(declRef->getDecl())) {
        auto varDecl = static_cast<clang::VarDecl *>(declRef->getDecl());
        dealVarDecl(varDecl, es, false);
      } else {
        continue;
      }
    }
    auto var = es->table.getIdentifer(varName);
    var->dataFlows.push_back(const_cast<clang::CallExpr *>(callExpr));
    var->dataFlowStrs.push_back(getStmtAsString(callExpr, context));
    var->isLocal = false;
    var->begDataFlows.push_back(
        getLineInfo(sourceManager, callExpr->getBeginLoc()));
    var->endDataFlows.push_back(
        getLineInfo(sourceManager, callExpr->getEndLoc()));  
  }
  
  int begLine =
      getLineInfo(this->sourceManager, callExpr->getBeginLoc()).second;
  int endLine =
      getLineInfo(this->sourceManager, callExpr->getBeginLoc()).second;
  
  for (auto identPair : es->table.getAllIdentifiers()) {
    auto name = identPair.first;
    if (visited.find(name) == visited.end()) {
      for (int i = begLine; i <= endLine; i++) {
        
        auto &lineTokens = (*lineTokMap)[i];
        if (std::find(lineTokens.begin(), lineTokens.end(), name) !=
            lineTokens.end()) {
          auto ident = es->table.getIdentifer(name);
          ident->dataFlows.push_back(const_cast<clang::CallExpr *>(callExpr));
          ident->dataFlowStrs.push_back(getStmtAsString(callExpr, context));
          ident->begDataFlows.push_back(
              getLineInfo(this->sourceManager, callExpr->getBeginLoc()));
          ident->endDataFlows.push_back(
              getLineInfo(this->sourceManager, callExpr->getBeginLoc()));
          ident->isLocal = false;
          break;
        }
      }
    }
  }

  int argCnt = 0;
  for (auto iter = callExpr->arg_begin(); iter != callExpr->arg_end(); iter++) {
    argCnt++;
  }
  if (argCnt != 0) {
    return;
  }

  es->globalCall.push_back(const_cast<clang::CallExpr *>(callExpr));
  es->globalCallStrs.push_back(getStmtAsString(callExpr, context));
  es->globalCallBegLoc.push_back(
      getLineInfo(this->sourceManager, callExpr->getBeginLoc()));
  es->globalCallEndLoc.push_back(
      getLineInfo(this->sourceManager, callExpr->getEndLoc()));
  
}


void ExecutionEngine::dealAssignOp(clang::Expr *l, clang::Expr *r,
                                   std::shared_ptr<ExecutionState> es) {
  VarDeclVisitor v{context};
  v.TraverseStmt(l);
  for (auto declRef : v.getDeclRefs()) {
    std::string lIdentifier = declRef->getNameInfo().getAsString();

    if (es->table.getIdentifer(lIdentifier)) {
      auto ident = es->table.getIdentifer(lIdentifier);
      ident->dataFlows.push_back(r);
      ident->dataFlowStrs.push_back(getStmtAsString(r, context));
      ident->begDataFlows.push_back(
          getLineInfo(this->sourceManager, l->getBeginLoc()));
      ident->endDataFlows.push_back(
          getLineInfo(this->sourceManager, l->getBeginLoc()));
      continue;
    }

    std::shared_ptr<Identifier> iPtr = std::make_shared<Identifier>();
    iPtr->qualType = declRef->getType();
    iPtr->dataFlows.push_back(r);
    iPtr->dataFlowStrs.push_back(getStmtAsString(r, context));
    iPtr->begDataFlows.push_back(
        getLineInfo(this->sourceManager, r->getBeginLoc()));
    iPtr->endDataFlows.push_back(
        getLineInfo(this->sourceManager, r->getBeginLoc()));
    iPtr->name = lIdentifier;
    

    VarDeclVisitor v1{this->context};
    v1.TraverseStmt(const_cast<clang::Expr *>(r));
    for (auto declRef : v1.getDeclRefs()) {

      iPtr->isLocal = iPtr->isLocal && ExecutionEngine::isPureLocal(
                                           declRef, this->sourceManager);
    }
    iPtr->isLocal = iPtr->isLocal &&
                    ExecutionEngine::isPureLocal(declRef, this->sourceManager);
    es->table.addIdentifier(iPtr);
  }

  VarDeclVisitor vr{context};
  vr.TraverseStmt(r);

  for (auto declRef : vr.getDeclRefs()) {
    std::string lIdentifier = declRef->getNameInfo().getAsString();
    if (es->table.getIdentifer(lIdentifier)) {
      auto ident = es->table.getIdentifer(lIdentifier);
      ident->dataFlows.push_back(r);
      ident->dataFlowStrs.push_back(getStmtAsString(r, context));
      ident->begDataFlows.push_back(
          getLineInfo(this->sourceManager, l->getBeginLoc()));
      ident->endDataFlows.push_back(
          getLineInfo(this->sourceManager, l->getBeginLoc()));
      continue;
    }
    std::shared_ptr<Identifier> iPtr = std::make_shared<Identifier>();
    iPtr->qualType = declRef->getType();
    iPtr->dataFlows.push_back(r);
    iPtr->dataFlowStrs.push_back(getStmtAsString(r, context));
    iPtr->begDataFlows.push_back(
        getLineInfo(this->sourceManager, r->getBeginLoc()));
    iPtr->endDataFlows.push_back(
        getLineInfo(this->sourceManager, r->getBeginLoc()));
    iPtr->name = lIdentifier;
    es->table.addIdentifier(iPtr);
  }
}

void ExecutionEngine::addToDataFlow(clang::Stmt *stmt,
                                    std::shared_ptr<ExecutionState> esPtr) {
  VarDeclVisitor vr{context};
  vr.TraverseStmt(stmt);
  std::unordered_set<std::string> visited;
  for (auto declRef : vr.getDeclRefs()) {
    std::string lIdentifier = declRef->getNameInfo().getAsString();
    visited.emplace(lIdentifier);
    if (esPtr->table.getIdentifer(lIdentifier)) {
      auto ident = esPtr->table.getIdentifer(lIdentifier);
      ident->dataFlows.push_back(stmt);
      ident->dataFlowStrs.push_back(getStmtAsString(stmt, context));
      ident->begDataFlows.push_back(
          getLineInfo(this->sourceManager, stmt->getBeginLoc()));
      ident->endDataFlows.push_back(
          getLineInfo(this->sourceManager, stmt->getBeginLoc()));
      continue;
    }

    std::shared_ptr<Identifier> iPtr = std::make_shared<Identifier>();
    iPtr->qualType = declRef->getType();
    iPtr->dataFlows.push_back(stmt);
    iPtr->dataFlowStrs.push_back(getStmtAsString(stmt, context));
    iPtr->begDataFlows.push_back(
        getLineInfo(this->sourceManager, stmt->getBeginLoc()));
    iPtr->endDataFlows.push_back(
        getLineInfo(this->sourceManager, stmt->getBeginLoc()));
    iPtr->name = lIdentifier;
    esPtr->table.addIdentifier(iPtr);
  }

  int begLine = getLineInfo(this->sourceManager, stmt->getBeginLoc()).second;
  int endLine = getLineInfo(this->sourceManager, stmt->getBeginLoc()).second;
  for (auto identPair : esPtr->table.getAllIdentifiers()) {
    auto name = identPair.first;
    if (visited.find(name) == visited.end()) {
      for (int i = begLine; i <= endLine; i++) {
        auto lineTokens = (*lineTokMap)[i];
        if (std::find(lineTokens.begin(), lineTokens.end(), name) !=
            lineTokens.end()) {
          auto ident = esPtr->table.getIdentifer(name);
          ident->dataFlows.push_back(stmt);
          ident->dataFlowStrs.push_back(getStmtAsString(stmt, context));
          ident->begDataFlows.push_back(
              getLineInfo(this->sourceManager, stmt->getBeginLoc()));
          ident->endDataFlows.push_back(
              getLineInfo(this->sourceManager, stmt->getBeginLoc()));
          break;
        }
      }
    }
  }
}

void ExecutionEngine::addToExeState(const clang::Stmt *stmt,
                                    std::shared_ptr<ExecutionState> esPtr) {
  if (stmt == nullptr) {
    return;
  }
 
  auto begInfo = getLineInfo(sourceManager, stmt->getBeginLoc());
  auto endInfo = getLineInfo(sourceManager, stmt->getEndLoc());
  std::string str = getStmtAsString(stmt, context);
  auto &blockState = esPtr->getTopBlockState();
  blockState.allBegs.push_back(begInfo);
  blockState.allEnds.push_back(endInfo);
  blockState.strPath.push_back(str);
}

void ExecutionEngine::addToNonLocal(clang::Stmt *stmt,
                                    std::shared_ptr<ExecutionState> es) {
  VarDeclVisitor v{context};
  v.TraverseStmt(stmt);
  for (auto declRef : v.getDeclRefs()) {
    std::string vname = declRef->getNameInfo().getAsString();
    if (es->table.getIdentifer(vname)) {
      auto ident = es->table.getIdentifer(vname);
      ident->isLocal = false;
      return;
    }
    std::shared_ptr<Identifier> iPtr = std::make_shared<Identifier>();
    iPtr->qualType = declRef->getType();
    iPtr->name = vname;
    iPtr->isLocal = false;
    es->table.addIdentifier(iPtr);
  }
}

std::shared_ptr<ExecutionState>
ExecutionEngine::execute(std::vector<clang::CFGBlock *> &path) {
  std::shared_ptr<ExecutionState> esPtr = std::make_shared<ExecutionState>();
  for (size_t i = 0; i < path.size(); ++i) {
    auto block = path[i];
    esPtr->blockStates.emplace_back(BlockState());
    auto &curBlockState = esPtr->getTopBlockState();
    curBlockState.curBlock = block;
    if (clang::Stmt *label = const_cast<clang::Stmt *>(block->getLabel())) {
      addToExeState(label, esPtr);
      if (clang::LabelStmt *L = clang::dyn_cast<clang::LabelStmt>(label)) {
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
        c->conExpr = L;
        c->conStr = L->getName();
        c->begLoc = getLineInfo(this->sourceManager, L->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, L->getBeginLoc());
        esPtr->constraints.addConstraint(c, block);
      } else if (clang::CaseStmt *caseStmt =
                     clang::dyn_cast<clang::CaseStmt>(label)) {
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
        c->conExpr = caseStmt->getLHS();
        c->conStr = getStmtAsString(c->conExpr, context);
        c->begLoc = getLineInfo(this->sourceManager, c->conExpr->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, c->conExpr->getEndLoc());
        esPtr->constraints.addConstraint(c, block);
      } else if (clang::DefaultStmt *defaultStmt =
                     clang::dyn_cast<clang::DefaultStmt>(label)) {
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
        c->conExpr = defaultStmt;
        c->conStr = getStmtAsString(c->conExpr, context);
        c->begLoc = getLineInfo(this->sourceManager, c->conExpr->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, c->conExpr->getBeginLoc());
        esPtr->constraints.addConstraint(c, block);
      } 
    }
    for (auto iter = block->begin(); iter != block->end(); iter++) {
      auto &cfgElement = *iter;
      if (cfgElement.getKind() == clang::CFGElement::ScopeBegin) {
        esPtr->table.enterScope();
      }

      if (cfgElement.getKind() == clang::CFGElement::Kind::Statement) {
        clang::CFGStmt CS = cfgElement.castAs<clang::CFGStmt>();
        const clang::Stmt *stmt = CS.getStmt();
        if (stmt->getStmtClass() == clang::Stmt::DeclStmtClass) {
          
          addToExeState(stmt, esPtr);
          auto declStmt = clang::cast<clang::DeclStmt>(stmt);
          if (declStmt->isSingleDecl()) {
            auto decl = declStmt->getSingleDecl();
            if (clang::VarDecl::classof(decl)) {
              dealVarDecl(clang::dyn_cast<clang::VarDecl>(decl), esPtr, true);
            }
          } else {
            auto declGroup = declStmt->getDeclGroup();
            for (auto iter = declGroup.begin(); iter != declGroup.end();
                 iter++) {
              auto decl = *iter;
              if (clang::VarDecl::classof(decl)) {
                dealVarDecl(clang::dyn_cast<clang::VarDecl>(decl), esPtr, true);
              }
            }
          }
        } else if (stmt->getStmtClass() == clang::Stmt::BinaryOperatorClass) {
          
          const clang::BinaryOperator *bOp =
              clang::dyn_cast<clang::BinaryOperator>(stmt);
          clang::BinaryOperator::Opcode opc;
          if (bOp->getOpcode() == clang::BO_Assign) {
            addToExeState(bOp, esPtr);
            clang::Expr *lExpr = bOp->getLHS();
            clang::Expr *rExpr = bOp->getRHS();
            dealAssignOp(lExpr, rExpr, esPtr);
            continue;
          } else if (bOp->getOpcode() == clang::BO_AddAssign) {
            opc = clang::BO_Add;
          } else if (bOp->getOpcode() == clang::BO_AndAssign) {
            opc = clang::BO_And;
          } else if (bOp->getOpcode() == clang::BO_DivAssign) {
            opc = clang::BO_Div;
          } else if (bOp->getOpcode() == clang::BO_MulAssign) {
            opc = clang::BO_Mul;
          } else if (bOp->getOpcode() == clang::BO_OrAssign) {
            opc = clang::BO_Or;
          } else if (bOp->getOpcode() == clang::BO_RemAssign) {
            opc = clang::BO_Rem;
          } else if (bOp->getOpcode() == clang::BO_ShlAssign) {
            opc = clang::BO_Shl;
          } else if (bOp->getOpcode() == clang::BO_ShrAssign) {
            opc = clang::BO_Shr;
          } else if (bOp->getOpcode() == clang::BO_SubAssign) {
            opc = clang::BO_Sub;
          } else if (bOp->getOpcode() == clang::BO_XorAssign) {
            opc = clang::BO_Xor;
          } else {
            continue;
          }

          addToExeState(bOp, esPtr);

          dealAssignOp(bOp->getLHS(), bOp->getRHS(), esPtr);

        } else if (stmt->getStmtClass() == clang::Stmt::UnaryOperatorClass) {
          const clang::UnaryOperator *uOp =
              clang::dyn_cast<clang::UnaryOperator>(stmt);

          clang::BinaryOperator::Opcode opc;
          if (uOp->getOpcode() == clang::UO_PostInc ||
              uOp->getOpcode() == clang::UO_PreInc) {
            opc = clang::BO_Add;
          } else if (uOp->getOpcode() == clang::UO_PostDec ||
                     uOp->getOpcode() == clang::UO_PreDec) {
            opc = clang::BO_Sub;
          } else {
            continue;
          }
          addToExeState(uOp, esPtr);

          dealAssignOp(uOp->getSubExpr(),
                       const_cast<clang::UnaryOperator *>(uOp), esPtr);
        } else if (stmt->getStmtClass() ==
                   clang::Stmt::CompoundAssignOperatorClass) {
          const clang::CompoundAssignOperator *cop =
              clang::dyn_cast<clang::CompoundAssignOperator>(stmt);

          addToExeState(cop, esPtr);
          dealAssignOp(cop->getLHS(),
                       const_cast<clang::CompoundAssignOperator *>(cop), esPtr);
        } else if (stmt->getStmtClass() == clang::Stmt::CallExprClass) {
          auto callStmt = clang::cast<clang::CallExpr>(stmt);
          dealFuncCall(callStmt, esPtr);
          addToExeState(callStmt, esPtr);
        } else if (stmt->getStmtClass() == clang::Stmt::ReturnStmtClass) {
          auto retStmt = clang::cast<clang::ReturnStmt>(stmt);
          addToNonLocal(const_cast<clang::ReturnStmt *>(retStmt), esPtr);
          addToExeState(retStmt, esPtr);
          curBlockState.hasRet = true;
          curBlockState.retStmt = retStmt;
        } else if (stmt->getStmtClass() == clang::Stmt::RecoveryExprClass) {

        } else {
          addToDataFlow(const_cast<clang::Stmt *>(stmt), esPtr);
        }
      }
    }

    if (i < path.size() && block->getTerminator().isValid()) {
      const auto &terminator = block->getTerminator();
      auto terStmt = terminator.getStmt();
      if (terStmt->getStmtClass() == clang::Stmt::IfStmtClass) {
        auto ifStmt = clang::cast<clang::IfStmt>(terStmt);
        auto cond = ifStmt->getCond();
        addToExeState(cond, esPtr);

        addToNonLocal(const_cast<clang::Expr *>(cond), esPtr);
        addToDataFlow(const_cast<clang::Expr *>(cond), esPtr);
        int condBegLine =
            getLineInfo(sourceManager, cond->getBeginLoc()).second;
        auto linfo = bm->getLineInfo(condBegLine, isPrev);
        if (linfo->lineStr.find("if") == std::string::npos) {
          continue;
        }
        auto then = ifStmt->getThen();

        if (then == NULL || then == nullptr || then == 0x0) {
          std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
          clang::Expr *notExpr = clang::UnaryOperator::Create(
              *context, const_cast<clang::Expr *>(cond), clang::UO_LNot,
              cond->getType(), cond->getValueKind(), cond->getObjectKind(),
              cond->getBeginLoc(), false,
              cond->getFPFeaturesInEffect(*langOpts));
          c->conExpr = notExpr;
          c->conStr = getStmtAsString(c->conExpr, context);
          c->begLoc = getLineInfo(sourceManager, cond->getBeginLoc());
          c->endLoc = getLineInfo(sourceManager, cond->getEndLoc());
          esPtr->constraints.addConstraint(c, block);
          continue;
        }

        int thenBegLine =
            getLineInfo(sourceManager, then->getBeginLoc()).second;
        int thenEndLine = getLineInfo(sourceManager, then->getEndLoc()).second;
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
        int nextBegLine = -1;
        if (i < path.size() - 1) {
          nextBegLine = binfoGen->getBegLine(path[i + 1]);
        }
        c->conExpr = const_cast<clang::Expr *>(cond);
        c->conStr = getStmtAsString(c->conExpr, context);

        if ((nextBegLine > thenBegLine && nextBegLine > thenEndLine) ||
            (nextBegLine < thenBegLine && nextBegLine < thenEndLine)) {
          c->conStr = "!" + c->conStr;
        }
        
        c->begLoc = getLineInfo(sourceManager, cond->getBeginLoc());
        c->endLoc = getLineInfo(sourceManager, cond->getEndLoc());
        
        esPtr->constraints.addConstraint(c, block);
      } else if (terStmt->getStmtClass() == clang::Stmt::WhileStmtClass) {
        
        auto whileStmt = clang::cast<clang::WhileStmt>(terStmt);
        auto cond = whileStmt->getCond();
        addToExeState(cond, esPtr);

        addToNonLocal(const_cast<clang::Expr *>(cond), esPtr);
        addToDataFlow(const_cast<clang::Expr *>(cond), esPtr);
        int condBegLine =
            getLineInfo(sourceManager, cond->getBeginLoc()).second;
        auto linfo = bm->getLineInfo(condBegLine, isPrev);
        if (linfo->lineStr.find("while") == std::string::npos) {
          continue;
        }
        auto body = whileStmt->getBody();

        if (body == NULL || body == nullptr || body == 0x0) {
          std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
          clang::Expr *notExpr = clang::UnaryOperator::Create(
              *context, const_cast<clang::Expr *>(cond), clang::UO_LNot,
              cond->getType(), cond->getValueKind(), cond->getObjectKind(),
              cond->getBeginLoc(), false,
              cond->getFPFeaturesInEffect(*langOpts));
          c->conExpr = notExpr;
          c->conStr = getStmtAsString(c->conExpr, context);
          c->begLoc = getLineInfo(this->sourceManager, cond->getBeginLoc());
          c->endLoc = getLineInfo(this->sourceManager, cond->getEndLoc());
          esPtr->constraints.addConstraint(c, block);
          continue;
        }

        int bodyBegLine =
            getLineInfo(sourceManager, body->getBeginLoc()).second;
        int bodyEndLine = getLineInfo(sourceManager, body->getEndLoc()).second;
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();

        clang::CFGBlock *notBodyBlock = nullptr;

        for (auto iter = block->succ_begin(); iter != block->succ_end();
             iter++) {
          if (*iter == NULL || *iter == nullptr || *iter == 0x0) {
            continue;
          }
          int begLine = binfoGen->getBegLine(*iter);
          if (begLine != -1 && bodyBegLine < begLine && bodyEndLine < begLine) {
            notBodyBlock = *iter;
            break;
          }
        }

        if (notBodyBlock != nullptr && i < path.size() - 1 &&
            notBodyBlock == path[i + 1]) {
          clang::Expr *notExpr = clang::UnaryOperator::Create(
              *context, const_cast<clang::Expr *>(cond), clang::UO_LNot,
              cond->getType(), cond->getValueKind(), cond->getObjectKind(),
              cond->getBeginLoc(), false,
              cond->getFPFeaturesInEffect(*langOpts));
          c->conExpr = notExpr;
        } else {
          c->conExpr = const_cast<clang::Expr *>(cond);
        }
        c->conStr = getStmtAsString(c->conExpr, context);
        c->begLoc = getLineInfo(this->sourceManager, cond->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, cond->getEndLoc());
        esPtr->constraints.addConstraint(c, block);
      } else if (terStmt->getStmtClass() == clang::Stmt::DoStmtClass) {
        
        auto doStmt = clang::cast<clang::DoStmt>(terStmt);
        auto cond = doStmt->getCond();
        addToExeState(cond, esPtr);
        addToNonLocal(const_cast<clang::Expr *>(cond), esPtr);
        addToDataFlow(const_cast<clang::Expr *>(cond), esPtr);
        int condBegLine =
            getLineInfo(sourceManager, cond->getBeginLoc()).second;
        auto linfo = bm->getLineInfo(condBegLine, isPrev);
        if (linfo->lineStr.find("while") == std::string::npos) {
          continue;
        } 
        auto body = doStmt->getBody();
       
        if (body == NULL || body == nullptr || body == 0x0) {
          std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
          clang::Expr *notExpr = clang::UnaryOperator::Create(
              *context, const_cast<clang::Expr *>(cond), clang::UO_LNot,
              cond->getType(), cond->getValueKind(), cond->getObjectKind(),
              cond->getBeginLoc(), false,
              cond->getFPFeaturesInEffect(*langOpts));
          c->conExpr = notExpr;
          c->conStr = getStmtAsString(c->conExpr, context);
          c->begLoc = getLineInfo(this->sourceManager, cond->getBeginLoc());
          c->endLoc = getLineInfo(this->sourceManager, cond->getEndLoc());
          esPtr->constraints.addConstraint(c, block);
          continue;
        }

        clang::CFGBlock *notBodyBlock = nullptr;
        int bodyBegLine =
            getLineInfo(sourceManager, body->getBeginLoc()).second;
        int bodyEndLine = getLineInfo(sourceManager, body->getEndLoc()).second;
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();

        for (auto iter = block->succ_begin(); iter != block->succ_end();
             iter++) {
          if (*iter == 0x0 || *iter == NULL || *iter == nullptr) {
            continue;
          }
          int begLine = binfoGen->getBegLine(*iter);
          if (begLine != -1 && bodyBegLine < begLine && bodyEndLine < begLine) {
            notBodyBlock = *iter;
            break;
          }
        }

        if (notBodyBlock != nullptr && i < path.size() - 1 &&
            notBodyBlock == path[i + 1]) {
          clang::Expr *notExpr = clang::UnaryOperator::Create(
              *context, const_cast<clang::Expr *>(cond), clang::UO_LNot,
              cond->getType(), cond->getValueKind(), cond->getObjectKind(),
              cond->getBeginLoc(), false,
              cond->getFPFeaturesInEffect(*langOpts));
          c->conExpr = notExpr;
        } else {
          c->conExpr = const_cast<clang::Expr *>(cond);
        }
        c->conStr = getStmtAsString(c->conExpr, context);
        c->begLoc = getLineInfo(this->sourceManager, cond->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, cond->getEndLoc());
        esPtr->constraints.addConstraint(c, block);
      } else if (terStmt->getStmtClass() == clang::Stmt::ForStmtClass) {
        auto forStmt = clang::cast<clang::ForStmt>(terStmt);
        int forBegLine =
            getLineInfo(this->sourceManager, forStmt->getBeginLoc()).second;
        auto lineno = bm->getLineInfo(forBegLine, isPrev);
        if (lineno->lineStr.find("for") == std::string::npos) {
          continue;
        }

        auto cond = forStmt->getCond();
        if (cond == NULL || cond == 0x0 || cond == nullptr) {
          std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
          c->conStr = "empty";
          c->begLoc = getLineInfo(this->sourceManager, forStmt->getBeginLoc());
          c->endLoc = getLineInfo(this->sourceManager, forStmt->getBeginLoc());
          esPtr->constraints.addConstraint(c, block);
          addToExeState(forStmt, esPtr);
          continue;
        }
        addToExeState(cond, esPtr);
        addToNonLocal(const_cast<clang::Expr *>(cond), esPtr);
        addToDataFlow(const_cast<clang::Expr *>(cond), esPtr);
        auto body = forStmt->getBody();

        if (body == NULL || body == 0x0 || body == nullptr) {
          std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
          clang::Expr *notExpr = clang::UnaryOperator::Create(
              *context, const_cast<clang::Expr *>(cond), clang::UO_LNot,
              cond->getType(), cond->getValueKind(), cond->getObjectKind(),
              cond->getBeginLoc(), false,
              cond->getFPFeaturesInEffect(*langOpts));
          c->conExpr = notExpr;
          c->conStr = getStmtAsString(c->conExpr, context);
          c->begLoc = getLineInfo(this->sourceManager, cond->getBeginLoc());
          c->endLoc = getLineInfo(this->sourceManager, cond->getEndLoc());
          esPtr->constraints.addConstraint(c, block);
          continue;
        }

        int bodyBegLine =
            getLineInfo(sourceManager, body->getBeginLoc()).second;
        int bodyEndLine = getLineInfo(sourceManager, body->getEndLoc()).second;
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();

        clang::CFGBlock *notBodyBlock = nullptr;

        for (auto iter = block->succ_begin(); iter != block->succ_end();
             iter++) {
          if (*iter == 0x0 || *iter == NULL || *iter == nullptr) {
            continue;
          }
          int begLine = binfoGen->getBegLine(*iter);
          if (begLine != -1 && bodyBegLine < begLine && bodyEndLine < begLine) {
            notBodyBlock = *iter;
            break;
          }
        }
        if (notBodyBlock != nullptr && i < path.size() - 1 &&
            notBodyBlock == path[i + 1]) {
          clang::Expr *notExpr = clang::UnaryOperator::Create(
              *context, const_cast<clang::Expr *>(cond), clang::UO_LNot,
              cond->getType(), cond->getValueKind(), cond->getObjectKind(),
              cond->getBeginLoc(), false,
              cond->getFPFeaturesInEffect(*langOpts));
          c->conExpr = notExpr;
        } else {
          c->conExpr = const_cast<clang::Expr *>(cond);
        }
        c->conStr = getStmtAsString(c->conExpr, context);
        c->begLoc = getLineInfo(this->sourceManager, cond->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, cond->getEndLoc());
        esPtr->constraints.addConstraint(c, block);
      } else if (terStmt->getStmtClass() == clang::Stmt::SwitchStmtClass) {
        auto switchStmt = clang::cast<clang::SwitchStmt>(terStmt);
        auto cond = switchStmt->getCond();
        if (cond == NULL || cond == 0x0 || cond == nullptr) {
          std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
          c->conStr = "empty";
          c->begLoc =
              getLineInfo(this->sourceManager, switchStmt->getBeginLoc());
          c->endLoc =
              getLineInfo(this->sourceManager, switchStmt->getBeginLoc());
          esPtr->constraints.addConstraint(c, block);
          addToExeState(switchStmt, esPtr);
          continue;
        }
        addToExeState(cond, esPtr);
        addToNonLocal(const_cast<clang::Expr *>(cond), esPtr);
        addToDataFlow(const_cast<clang::Expr *>(cond), esPtr);

        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
        c->conExpr = const_cast<clang::Expr *>(cond);
        c->conStr = getStmtAsString(c->conExpr, context);
        c->begLoc = getLineInfo(this->sourceManager, cond->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, cond->getEndLoc());
        esPtr->constraints.addConstraint(c, block);
      } else if (terStmt->getStmtClass() == clang::Stmt::BreakStmtClass ||
                 terStmt->getStmtClass() == clang::Stmt::ContinueStmtClass ||
                 terStmt->getStmtClass() == clang::Stmt::GotoStmtClass) {
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
        c->conExpr = const_cast<clang::Stmt *>(terStmt);
        c->conStr = getStmtAsString(c->conExpr, context);
        c->begLoc = getLineInfo(this->sourceManager, terStmt->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, terStmt->getEndLoc());
        esPtr->constraints.addConstraint(c, block);
      } else {
        int terStmtBegLine =
            getLineInfo(sourceManager, terStmt->getBeginLoc()).second;
        addToDataFlow(const_cast<clang::Stmt *>(terStmt), esPtr);
        auto lineInfo = bm->getLineInfo(terStmtBegLine, isPrev);
        if (!((lineInfo->lineStr.find("||") != std::string::npos) ||
              (lineInfo->lineStr.find("&&") != std::string::npos) ||
              (lineInfo->lineStr.find("!") != std::string::npos))) {
          continue;
        }
        
        bool isNotCond = false;
        auto condBlock = *block->succ_begin();
        if (condBlock != nullptr &&
            !(path[i + 1] == condBlock.getReachableBlock())) {
          isNotCond = true;
        }
        std::shared_ptr<Constraint> c = std::make_shared<Constraint>();
        c->begLoc = getLineInfo(this->sourceManager, terStmt->getBeginLoc());
        c->endLoc = getLineInfo(this->sourceManager, terStmt->getEndLoc());
        if (isNotCond) {
          c->conStr = "!" + getStmtAsString(terStmt, context);
        } else {
          c->conStr = getStmtAsString(terStmt, context);
        }
        c->conExpr = const_cast<clang::Stmt *>(terStmt);
        esPtr->constraints.addConstraint(c, block);
        addToExeState(terStmt, esPtr);
      }
    }
  }
  return esPtr;
}