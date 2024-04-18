#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/CFG.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileEntry.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/FileSystemOptions.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/PreprocessorOutputOptions.h"
#include "clang/Frontend/Utils.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/ModuleLoader.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Lex/Token.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Sema/CodeCompleteConsumer.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaConsumer.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/JSON.h"
#include "llvm/TargetParser/Host.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct LineInfo {
  int lineno;
  std::string lineStr;
  bool isMod;
  std::string cid;
};

struct DeclInfo {
  bool isFuncDecl;
  std::string declName;
  std::vector<int> modLines;
  std::vector<LineInfo> lines;
};

struct FileInfo {
  std::string patch_fileName;
  std::string simple_fileName;
  std::string before_file_path;
  std::string before_dir_path;
  std::string before_pch_path;
  std::vector<std::string> before_mod_decls;
  std::string after_file_path;
  std::string after_dir_path;
  std::string after_pch_path;
  std::vector<std::string> after_mod_decls;
  std::vector<std::vector<DeclInfo>> all_decl_info;
};

FileInfo getFileInfo(std::string infoFilePath) {
  FileInfo fileInfo;
  std::ifstream inputFile(infoFilePath);
  if (!inputFile.is_open()) {
    std::cerr << "can not open file " << infoFilePath << std::endl;
    exit(1);
  }
  std::stringstream buffer;
  buffer << inputFile.rdbuf();
  inputFile.close();
  std::string fileContent = buffer.str();
  llvm::Expected<llvm::json::Value> e = llvm::json::parse(fileContent);
  llvm::json::Object *o = e->getAsObject();

  assert(o != nullptr);

  std::optional<llvm::StringRef> patch_fileName_ref =
      o->getString("patch_fileName");
  assert(patch_fileName_ref.has_value());
  fileInfo.patch_fileName = patch_fileName_ref.value().str();

  std::optional<llvm::StringRef> simple_fileName_ref =
      o->getString("simple_fileName");
  assert(simple_fileName_ref.has_value());
  fileInfo.simple_fileName = simple_fileName_ref.value().str();
  
  std::optional<llvm::StringRef> before_file_path_ref =
      o->getString("before_file_path");
  assert(before_file_path_ref.has_value());
  fileInfo.before_file_path = before_file_path_ref.value().str();
  
  std::optional<llvm::StringRef> before_dir_path_ref =
      o->getString("before_dir_path");
  assert(before_dir_path_ref.has_value());
  fileInfo.before_dir_path = before_dir_path_ref.value().str();
  
  std::optional<llvm::StringRef> before_pch_path_ref =
      o->getString("before_pch_path");
  assert(before_pch_path_ref.has_value());
  fileInfo.before_pch_path = before_pch_path_ref.value().str();

  std::optional<llvm::StringRef> after_file_path_ref =
      o->getString("after_file_path");
  assert(after_file_path_ref.has_value());
  fileInfo.after_file_path = after_file_path_ref.value().str();
  
  std::optional<llvm::StringRef> after_dir_path_ref =
      o->getString("after_dir_path");
  assert(after_dir_path_ref.has_value());
  fileInfo.after_dir_path = after_dir_path_ref.value().str();

  std::optional<llvm::StringRef> after_pch_path_ref =
      o->getString("after_pch_path");
  assert(after_pch_path_ref.has_value());
  fileInfo.after_pch_path = after_pch_path_ref.value().str();

  auto before_mod_decls = o->getArray("before_mod_decls");
  assert(before_mod_decls);
  for (auto modIter = before_mod_decls->begin();
       modIter != before_mod_decls->end(); modIter++) {
    auto mod_decl = modIter->getAsString();
    assert(mod_decl.hasValue());
    fileInfo.before_mod_decls.push_back(mod_decl.value().str());
  }

  auto after_mod_decls = o->getArray("after_mod_decls");
  assert(after_mod_decls);
  for (auto modIter = after_mod_decls->begin();
       modIter != after_mod_decls->end(); modIter++) {
    auto mod_decl = modIter->getAsString();
    assert(mod_decl.hasValue());
    fileInfo.after_mod_decls.push_back(mod_decl.value().str());
  }
  
  auto decl_infos = o->getArray("decl_infos");
  assert(decl_infos);
  for (auto iter = decl_infos->begin(); iter != decl_infos->end(); iter++) {
    auto decl_info_pair = iter->getAsArray();
    assert(decl_info_pair->size() == 2);
    std::vector<DeclInfo> declInfoPair;
    for (auto decl_iter = decl_info_pair->begin();
         decl_iter != decl_info_pair->end(); decl_iter++) {

      DeclInfo declInfo;
      auto decl_object = decl_iter->getAsObject();
      std::string decl_name = decl_object->getString("decl_name").value().str();
      bool is_func = decl_object->getBoolean("is_func").value();

      declInfo.declName = decl_name;
      declInfo.isFuncDecl = is_func;

      auto mod_lines_array = decl_object->getArray("mod_lines");
      for (auto mod_lines_iter = mod_lines_array->begin();
           mod_lines_iter != mod_lines_array->end(); mod_lines_iter++) {
        int lineno = mod_lines_iter->getAsInteger().value();
        declInfo.modLines.push_back(lineno);
      }

      auto line_info_array = decl_object->getArray("line_info");
      for (auto line_info_iter = line_info_array->begin();
           line_info_iter != line_info_array->end(); line_info_iter++) {
        auto line_info_object = line_info_iter->getAsObject();
        LineInfo linfo;
        linfo.lineno = line_info_object->getInteger("lineno").value();
        linfo.lineStr = line_info_object->getString("line_str").value();
        linfo.isMod = line_info_object->getBoolean("is_mod").value();
        linfo.cid = line_info_object->getString("cid").value();
        declInfo.lines.push_back(linfo);
      }

      declInfoPair.push_back(declInfo);
    }
    fileInfo.all_decl_info.push_back(declInfoPair);
  }
  return fileInfo;
}

class MySemaHandler : public clang::ExternalSemaSource {
public:
  MySemaHandler(clang::Sema *s) : sema(s), context(s->getASTContext()) {}
  ~MySemaHandler() = default;
  virtual bool LookupUnqualified(clang::LookupResult &R, clang::Scope *S) {
    clang::DeclarationName name = R.getLookupName();

    clang::IdentifierInfo *II = name.getAsIdentifierInfo();
    clang::SourceLocation Loc = R.getNameLoc();
    clang::VarDecl *Result = clang::VarDecl::Create(
        context, R.getSema().getFunctionLevelDeclContext(true), Loc, Loc, II,
        context.DependentTy,
        nullptr, clang::SC_None);
    if (Result) {
      R.addDecl(Result);
    }
    return true;
  }

  virtual void ReadUndefinedButUsed(
      llvm::MapVector<clang::NamedDecl *, clang::SourceLocation> &Undefined) {
    for (auto &p : Undefined) {
      auto namedDecl = p.first;
    }
  }

private:
  clang::Sema *sema;
  clang::ASTContext &context;
};

class CustomConsumer : public clang::ASTConsumer {
  std::unordered_map<std::string, clang::FunctionDecl *> declMap;
  std::unordered_map<std::string, std::unique_ptr<clang::CFG>> cfgMap;
  clang::ASTContext *astContext;

public:
  CustomConsumer(clang::ASTContext *astContext) {
    this->astContext = astContext;
  }
  virtual bool HandleTopLevelDecl(clang::DeclGroupRef decls) override {
    clang::DeclGroupRef::iterator it;
    for (it = decls.begin(); it != decls.end(); it++) {
      clang::FunctionDecl *fd = llvm::dyn_cast<clang::FunctionDecl>(*it);
      if (fd) {
        declMap[fd->getNameAsString()] = fd;
      }
    }
    return true;
  }

  clang::CFG *getCfgByName(std::string funcName) {
    assert(declMap.find(funcName) != declMap.end());
    auto funcDecl = declMap[funcName];
    clang::CFG::BuildOptions buildOpts;
    std::unique_ptr<clang::CFG> cfgPtr = clang::CFG::buildCFG(
        funcDecl, funcDecl->getBody(), astContext, buildOpts);
    cfgMap[funcName] = std::move(cfgPtr);
    return cfgMap[funcName].get();
  }

  clang::FunctionDecl *getFuncDeclByName(std::string funcName) {
    assert(declMap.find(funcName) != declMap.end());

    auto funcDecl = declMap[funcName];
    return funcDecl;
  }
};

class CompoundStmtVisitor
    : public clang::RecursiveASTVisitor<CompoundStmtVisitor> {
private:
  clang::ASTContext *context;
  std::vector<clang::CompoundStmt *> CompoundStmts;

public:
  explicit CompoundStmtVisitor(clang::ASTContext *c) : context(c) {}

  bool VisitCompoundStmt(clang::CompoundStmt *cs) {
    CompoundStmts.push_back(cs);
    return true;
  }
  std::vector<clang::CompoundStmt *> &getCompoundStmts() {
    return CompoundStmts;
  }
};

struct MYBlockStmt {
  int begLine;
  int endLine;
};

std::pair<std::string, int> getLineInfo(clang::SourceManager *sm,
                                        clang::SourceLocation sl) {
  clang::PresumedLoc pLoc = sm->getPresumedLoc(sl);
  assert(pLoc.isValid());
  return std::make_pair<std::string, int>("", pLoc.getLine());
}

// enter your path here, the path should be the same with the ASZZ_RESULT variable in settings.py in cids_without_dels directory 
const std::string resultPath =
    ".../aszz-result.json";

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    return -1;
  }

  FileInfo fileInfo = getFileInfo(argv[1]);
  llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> curDiagIDs(
      new clang::DiagnosticIDs());
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> curDiagOpts(
      new clang::DiagnosticOptions());

  clang::IgnoringDiagConsumer *curCustomDiagnosticConsumer =
      new clang::IgnoringDiagConsumer();
  clang::DiagnosticsEngine curDiagnostics(curDiagIDs, curDiagOpts,
                                          curCustomDiagnosticConsumer);

  clang::LangOptions curLangOpts;
  std::shared_ptr<clang::TargetOptions> curTo =
      std::make_shared<clang::TargetOptions>();
  curTo->Triple = llvm::sys::getDefaultTargetTriple();

  clang::TargetInfo *curPti =
      clang::TargetInfo::CreateTargetInfo(curDiagnostics, curTo);
  clang::FileSystemOptions curFsopts;
  clang::FileManager curFileManager(curFsopts);
  clang::SourceManager curSourceManager(curDiagnostics, curFileManager);
  std::shared_ptr<clang::HeaderSearchOptions> curHsopts =
      std::make_shared<clang::HeaderSearchOptions>();

  clang::HeaderSearch curHeaderSearch(curHsopts, curSourceManager,
                                      curDiagnostics, curLangOpts, curPti);

  std::shared_ptr<clang::PreprocessorOptions> curPpopts =
      std::make_shared<clang::PreprocessorOptions>();
  clang::TrivialModuleLoader curTrivialModuleLoader;
  clang::Preprocessor curPreprocessor(curPpopts, curDiagnostics, curLangOpts,
                                      curSourceManager, curHeaderSearch,
                                      curTrivialModuleLoader);

  curPreprocessor.Initialize(*curPti);
  clang::FileEntryRef &curFileEntryRef =
      curFileManager.getFileRef(fileInfo.after_file_path).get();
  curSourceManager.setMainFileID(curSourceManager.createFileID(
      curFileEntryRef, clang::SourceLocation{}, clang::SrcMgr::C_User));

  clang::IdentifierTable curIdents;
  clang::SelectorTable curSels;
  clang::Builtin::Context curBuiltins;
  clang::ASTContext curAstContext(curLangOpts, curSourceManager, curIdents,
                                  curSels, curBuiltins,
                                  clang::TranslationUnitKind::TU_Prefix);
  curAstContext.InitBuiltinTypes(*curPti);

  std::shared_ptr<CustomConsumer> curASTConsumer =
      std::make_shared<CustomConsumer>(&curAstContext);
  std::string curCodeCompletionStr;
  llvm::raw_string_ostream curStream(curCodeCompletionStr);
  clang::PrintingCodeCompleteConsumer curPrintingCodeCompleteConsumer(
      clang::CodeCompleteOptions{}, curStream);
  curASTConsumer->Initialize(curAstContext);
  curCustomDiagnosticConsumer->BeginSourceFile(curLangOpts, &curPreprocessor);

  clang::Sema *curS = new clang::Sema(
      curPreprocessor, curAstContext, *curASTConsumer,
      clang::TranslationUnitKind::TU_Prefix, &curPrintingCodeCompleteConsumer);
  MySemaHandler curMySemaHandler(curS);
  curS->addExternalSource(&curMySemaHandler);
  clang::ParseAST(*curS, false, false);

  std::vector<MYBlockStmt> blockStmts;
  std::vector<int> finalLines;
  for (auto &declInfo : fileInfo.all_decl_info) {
    auto &curDeclInfo = declInfo[1];
    clang::FunctionDecl *funcDecl =
        curASTConsumer->getFuncDeclByName(curDeclInfo.declName);
    CompoundStmtVisitor csv{&curAstContext};
    csv.TraverseStmt(funcDecl->getBody());
    auto css = csv.getCompoundStmts();
    for (auto cs : css) {
      int begLine = getLineInfo(&curSourceManager, cs->getBeginLoc()).second;
      int endLine = getLineInfo(&curSourceManager, cs->getEndLoc()).second;
      MYBlockStmt bs;
      bs.begLine = begLine;
      bs.endLine = endLine;
      blockStmts.push_back(bs);
    }
    std::sort(blockStmts.begin(), blockStmts.end(),
              [](auto &p1, auto &p2) { return p1.begLine < p2.begLine; });

    for (auto modLine : curDeclInfo.modLines) {
      int i = 0;
      for (; i < blockStmts.size();) {
        if (blockStmts[i].begLine <= modLine &&
            blockStmts[i].endLine >= modLine) {
          i++;
        } else {
          break;
        }
      }
      i--;
      if (i >= 0 && i < blockStmts.size()) {
        for (int j = blockStmts[i].begLine; j <= blockStmts[i].endLine; j++) {
          finalLines.push_back(j);
        }
      }
    }
    auto tmp = finalLines;
    finalLines.clear();
    for (auto l : tmp) {
      if (std::find(curDeclInfo.modLines.begin(), curDeclInfo.modLines.end(),
                    l) == curDeclInfo.modLines.end()) {
        finalLines.push_back(l);
      }
    }
  }

  std::string str;
  llvm::raw_string_ostream stream(str);
  llvm::json::OStream J(stream);

  J.array([&] {
    for (auto l : finalLines) {
      J.value(l);
    }
  });

  std::fstream f;
  f.open(resultPath, std::ios::out);
  f << str;
  f.close();
  return 0;
}