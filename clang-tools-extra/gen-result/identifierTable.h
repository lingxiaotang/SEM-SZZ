#ifndef MY_IDENTIFIER_TABLE
#define MY_IDENTIFIER_TABLE
#include "clang/AST/Expr.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Identifier;

class IdentifierTable {
private:
  std::vector<std::unordered_map<std::string, std::shared_ptr<Identifier>>>
      table;

public:
  IdentifierTable() = default;
  void enterScope();
  void exitScope();
  std::shared_ptr<Identifier> getIdentifer(std::string);
  void addIdentifier(std::shared_ptr<Identifier> &ident);

  std::unordered_map<std::string, std::shared_ptr<Identifier>>
  getAllIdentifiers();
};

struct Identifier {
  std::string name;
  clang::QualType qualType;
  std::pair<std::string, int> begDecl, endDecl;

  // std::vector<clang::Expr *>valueExprs;
  // std::vector<std::string> valueExprStrs;
  // std::vector<std::pair<std::string, int>> valueExprBegs, valueExprEnds;

  // std::vector<clang::CallExpr *> funcs;
  // std::vector<std::string> funcsStr;
  // std::vector<std::pair<std::string, int>> begFuncs;
  // std::vector<std::pair<std::string, int>> endFuncs;

  std::vector<clang::Stmt *> dataFlows;
  std::vector<std::string> dataFlowStrs;
  std::vector<std::pair<std::string, int>> begDataFlows;
  std::vector<std::pair<std::string, int>> endDataFlows;


  bool isLocal = true;

  Identifier() = default;
  Identifier(const std::string &n, clang::QualType q,
             std::vector<clang::Expr *> es, clang::SourceLocation s,
             bool isLocal);
  Identifier &operator=(const Identifier &ident);
  bool operator==(const Identifier &ident) const;

  bool isPureLocal() const;
};

struct HashIdentifier {
  size_t operator()(const Identifier &ident) const {
    return std::hash<std::string>()(ident.name);
  }
};

bool operator==(const clang::Expr &e1, const clang::Expr &e2);

std::string getExprAsString(const clang::Expr *expr);

#endif