#ifndef MY_CONSTRAINT
#define MY_CONSTRAINT
#include "constraint.h"
#include "identifierTable.h"

Constraint::Constraint(clang::Expr *e, std::pair<std::string, int> begLoc,
                       std::pair<std::string, int> endLoc) {
  this->conExpr = e;
  this->begLoc = begLoc;
  this->endLoc = endLoc;
  this->conStr = getExprAsString(e);
}
Constraint &Constraint::operator=(const Constraint &con) {
  this->conExpr = con.conExpr;
  this->conStr = con.conStr;
  this->begLoc = con.begLoc;
  this->endLoc = con.endLoc;
  this->identSet = con.identSet;
  return *this;
}
void Constraint::addIdent(std::shared_ptr<Identifier> ident) {
  this->identSet.emplace(ident->name, ident);
}

bool Constraint::operator==(const Constraint &con) {
  return this->conStr == con.conStr;
}

std::vector<std::shared_ptr<Constraint>> &AllConstraint::getAllConstraints() {
  return this->allConstraints;
}
void AllConstraint::addConstraint(std::shared_ptr<Constraint> &constraint,
                                  clang::CFGBlock *cfgBlock) {
  constraint->cfgBlock = cfgBlock;
  this->allConstraints.push_back(constraint);
}
bool AllConstraint::operator==(AllConstraint &constraints) {
  for (auto &c1 : this->allConstraints) {
    for (auto &c2 : constraints.getAllConstraints()) {
      if (!(c1->conStr == c2->conStr)) {
        return false;
      }
    }
  }
  return true;
}
std::string AllConstraint::toString() {
  std::string ret;
  for (size_t i = 0; i < this->allConstraints.size(); i++) {
    if (i == this->allConstraints.size() - 1) {
      ret = ret + "(" + this->allConstraints[i]->conStr + ")";
    } else {
      ret = ret + "(" + this->allConstraints[i]->conStr + ")&&";
    }
  }
  return ret;
}
void AllConstraint::addIdent(std::shared_ptr<Identifier> ident) {
  this->identSet.emplace(ident->name, ident);
}
std::unordered_map<std::string, std::shared_ptr<Identifier>> &
AllConstraint::getVarSet() {
  return this->identSet;
}
#endif