#include "identifierTable.h"
#include "clang/AST/PrettyPrinter.h"
#include "clang/Basic/LangOptions.h"
void IdentifierTable::enterScope() {
  // this->table.push_back(
  //     std::unordered_map<std::string, std::shared_ptr<Identifier>>{});
}

void IdentifierTable::exitScope() {
  // this->table.pop_back();
}

std::shared_ptr<Identifier> IdentifierTable::getIdentifer(std::string ident) {
  for (auto rIter = this->table.rbegin(); rIter != this->table.rend();
       rIter++) {
    if (rIter->count(ident) > 0) {
      return (*rIter)[ident];
    }
  }
  return nullptr;
}

void IdentifierTable::addIdentifier(std::shared_ptr<Identifier> &ident) {
  // assert(this->table.size() > 0);
  if (this->table.size() == 0) {
    this->table.push_back(
        std::unordered_map<std::string, std::shared_ptr<Identifier>>{});
  }

  this->table[this->table.size() - 1].emplace(ident->name, ident);
}

std::unordered_map<std::string, std::shared_ptr<Identifier>>
IdentifierTable::getAllIdentifiers() {
  std::unordered_map<std::string, std::shared_ptr<Identifier>> idents;
  for (auto iter = table.begin(); iter != table.end(); iter++) {
    for (auto &p : *iter) {
      idents.emplace(p);
    }
  }
  return idents;
}



bool Identifier::isPureLocal() const { return this->isLocal; }

std::string getExprAsString(const clang::Expr *expr) {
  clang::LangOptions opts;
  clang::PrintingPolicy printPolicy(opts);
  std::string exprString;
  llvm::raw_string_ostream stream(exprString);
  expr->printPretty(stream, nullptr, printPolicy);
  stream.flush();
  return exprString;
}

// 暂时这么写，之后会修改使之更加sound
bool operator==(const clang::Expr &e1, const clang::Expr &e2) {
  return getExprAsString(&e1) == getExprAsString(&e2);
}