#ifndef SYMBOLIC_ENGINE_H_
#define SYMBOLIC_ENGINE_H_
#include "BlockInfoGenerator.h"
#include "BlockMapper.h"
#include "PathGenerator.h"
#include "constraint.h"
#include "utility"
#include "vector"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"
// string represents file name while int represents line number
struct BlockState {
  clang::CFGBlock *curBlock;
  std::vector<std::string> strPath;
  std::vector<std::pair<std::string, int>> allBegs;
  std::vector<std::pair<std::string, int>> allEnds;

  bool hasRet = false;
  const clang::ReturnStmt *retStmt = nullptr;
};

struct ExecutionState {
  std::vector<BlockState> blockStates;
  std::vector<clang::CallExpr *> globalCall;
  std::vector<std::string> globalCallStrs;
  std::vector<std::pair<std::string, int>> globalCallBegLoc;
  std::vector<std::pair<std::string, int>> globalCallEndLoc;

  AllConstraint constraints;
  IdentifierTable table;

  ExecutionState() = default;

  size_t getBlockIndex(clang::CFGBlock *cfg);

  BlockState &getTopBlockState();
};

class ExecutionEngine {
  clang::ASTContext *context;
  clang::LangOptions *langOpts;
  clang::SourceManager *sourceManager;
  clang::SourceRange sourceRange;
  BlockMapper *bm;
  BlockInfoGenerator *binfoGen;
  bool isPrev;
  std::unordered_map<int, std::vector<std::string>> *lineTokMap;

  void dealVarDecl(const clang::VarDecl *varDecl,
                   std::shared_ptr<ExecutionState> es, bool addVarLoc);

  void dealFuncCall(const clang::CallExpr *callExpr,
                    std::shared_ptr<ExecutionState> es);

  void dealAssignOp(clang::Expr *l, clang::Expr *r,
                    std::shared_ptr<ExecutionState> es);
  int getBegLine(clang::CFGBlock *b);
  int getEndLine(clang::CFGBlock *b);

public:
  explicit ExecutionEngine(
      clang::ASTContext *c, clang::LangOptions *l, clang::SourceManager *s,
      BlockMapper *bm, BlockInfoGenerator *binfoGen, bool isPrev,
      std::unordered_map<int, std::vector<std::string>> &lineTokMap);
  std::shared_ptr<ExecutionState> execute(std::vector<clang::CFGBlock *> &path);
  bool isPureLocal(clang::DeclRefExpr *D, clang::SourceManager *sm);
  static std::pair<std::string, int> getLineInfo(clang::SourceManager *sm,
                                                 clang::SourceLocation sl);
  static std::string getStmtAsString(const clang::Stmt *stmt,
                                     clang::ASTContext *context);
  clang::ASTContext *getContext() { return context; }
  clang::LangOptions *getLangOpts() { return langOpts; }
  clang::SourceManager *getSourceManager() { return sourceManager; }
  clang::SourceRange getSourceRange() { return sourceRange; }

  void addToExeState(const clang::Stmt *stmt,
                     std::shared_ptr<ExecutionState> esPtr);
  void addToNonLocal(clang::Stmt *stmt, std::shared_ptr<ExecutionState> esPtr);
  void addToDataFlow(clang::Stmt *stmt, std::shared_ptr<ExecutionState> esPtr);
};

class VarDeclVisitor : public clang::RecursiveASTVisitor<VarDeclVisitor> {
private:
  clang::ASTContext *context;
  std::vector<clang::DeclRefExpr *> decls;

public:
  explicit VarDeclVisitor(clang::ASTContext *c);
  bool VisitDeclRefExpr(clang::DeclRefExpr *D);
  std::vector<clang::DeclRefExpr *> &getDeclRefs();
};
#endif