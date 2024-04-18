#ifndef MY_CONSTRAINT_H
#define MY_CONSTRAINT_H
#include "identifierTable.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/Analysis/CFG.h"
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
struct Constraint {
  std::string conStr;
  clang::Stmt *conExpr;
  std::unordered_map<std::string, std::shared_ptr<Identifier>> identSet;
  std::pair<std::string, int> begLoc;
  std::pair<std::string, int> endLoc;
  // deal in execution engine later
  clang::CFGBlock *cfgBlock = nullptr;

  Constraint() = default;
  Constraint(clang::Expr *e, std::pair<std::string, int> begLoc,
                         std::pair<std::string, int> endLoc);
  Constraint &operator=(const Constraint &con);
  bool operator==(const Constraint &con);
  void addIdent(std::shared_ptr<Identifier> ident);
};

class AllConstraint {
private:
  std::vector<std::shared_ptr<Constraint>> allConstraints;
  bool simpleCompare;
  std::unordered_map<std::string, std::shared_ptr<Identifier>> identSet;

public:
  AllConstraint() = default;
  std::vector<std::shared_ptr<Constraint>> &getAllConstraints();
  void addConstraint(std::shared_ptr<Constraint> &constraint,
                     clang::CFGBlock *cfgBlock);
  bool operator==(AllConstraint &constraints);
  std::string toString();
  void addIdent(std::shared_ptr<Identifier> ident);
  std::unordered_map<std::string, std::shared_ptr<Identifier>> &getVarSet();
};

#endif