#include "BlockInfoGenerator.h"
#include "BlockMapper.h"
#include "PathComparator.h"
#include "PathGenerator.h"
#include "StateCollector.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
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
};

// enter your path here, the path should be the same with the GEN_RESULT_PATH variable in settings.py in cids_without_dels directory 
const std::string resultPath =
    "";

std::unordered_map<int, std::vector<std::string>>
getLineTokens(std::string filePath) {
  llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> prevDiagIDs(
      new clang::DiagnosticIDs());
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> prevDiagOpts(
      new clang::DiagnosticOptions());
  clang::IgnoringDiagConsumer *prevCustomDiagnosticConsumer =
      new clang::IgnoringDiagConsumer();
  clang::DiagnosticsEngine prevDiagnostics(prevDiagIDs, prevDiagOpts,
                                           prevCustomDiagnosticConsumer);

  clang::LangOptions prevLangOpts;
  std::shared_ptr<clang::TargetOptions> prevTo =
      std::make_shared<clang::TargetOptions>();
  prevTo->Triple = llvm::sys::getDefaultTargetTriple();

  clang::TargetInfo *prevPti =
      clang::TargetInfo::CreateTargetInfo(prevDiagnostics, prevTo);
  clang::FileSystemOptions prevFsopts;
  clang::FileManager prevFileManager(prevFsopts);
  clang::SourceManager prevSourceManager(prevDiagnostics, prevFileManager);
  std::shared_ptr<clang::HeaderSearchOptions> prevHsopts =
      std::make_shared<clang::HeaderSearchOptions>();

  clang::HeaderSearch prevHeaderSearch(prevHsopts, prevSourceManager,
                                       prevDiagnostics, prevLangOpts, prevPti);

  std::shared_ptr<clang::PreprocessorOptions> prevPpopts =
      std::make_shared<clang::PreprocessorOptions>();
  clang::TrivialModuleLoader prevTrivialModuleLoader;

  clang::Preprocessor prevPreprocessor(
      prevPpopts, prevDiagnostics, prevLangOpts, prevSourceManager,
      prevHeaderSearch, prevTrivialModuleLoader);

  prevPreprocessor.Initialize(*prevPti);
  clang::FileEntryRef &prevFileEntryRef =
      prevFileManager.getFileRef(filePath).get();
  prevSourceManager.setMainFileID(prevSourceManager.createFileID(
      prevFileEntryRef, clang::SourceLocation{}, clang::SrcMgr::C_User));

  clang::IdentifierTable prevIdents;
  clang::SelectorTable prevSels;
  clang::Builtin::Context prevBuiltins;
  clang::ASTContext prevAstContext(prevLangOpts, prevSourceManager, prevIdents,
                                   prevSels, prevBuiltins,
                                   clang::TranslationUnitKind::TU_Prefix);
  prevAstContext.InitBuiltinTypes(*prevPti);

  std::shared_ptr<CustomConsumer> prevASTConsumer =
      std::make_shared<CustomConsumer>(&prevAstContext);
  std::string prevCodeCompletionStr;
  llvm::raw_string_ostream prevStream(prevCodeCompletionStr);
  clang::PrintingCodeCompleteConsumer prevPrintingCodeCompleteConsumer(
      clang::CodeCompleteOptions{}, prevStream);
  prevASTConsumer->Initialize(prevAstContext);
  prevCustomDiagnosticConsumer->BeginSourceFile(prevLangOpts,
                                                &prevPreprocessor);
  prevPreprocessor.EnterMainSourceFile();
  std::unordered_map<int, std::vector<std::string>> retMap;
  while (true) {
    clang::Token token;
    prevPreprocessor.Lex(token);
    if (token.is(clang::tok::eof)) {
      break;
    } else {
      auto begChar = prevSourceManager.getCharacterData(token.getLocation());
      std::string tokStr(begChar, begChar + token.getLength());
      int lineno =
          ExecutionEngine::getLineInfo(&prevSourceManager, token.getLocation())
              .second;
      retMap[lineno].push_back(tokStr);
    }
  }
  return retMap;
}

std::string trim(std::string &s) {
  std::string ret;
  if (s.empty()) {
    return s;
  }
  ret = s;
  ret.erase(0, ret.find_first_not_of(" \n\r\t"));
  ret.erase(ret.find_last_not_of(" \n\r\t") + 1);

  return ret;
}

struct pathInfo {
  std::string patchFileName;
  std::string funcDeclName;
  std::vector<int> curPath;
  std::vector<std::string> cids;
  bool isCond;
};

std::vector<int> filterDuplicate(std::vector<int> &linenos) {
  std::vector<int> ret;
  std::unordered_set<int> visited;
  for (auto lineno : linenos) {
    if (visited.find(lineno) != visited.end()) {
      continue;
    }
    visited.emplace(lineno);
    ret.push_back(lineno);
  }
  return ret;
}

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    return -1;
  }

  BlockMapper bm{argv[1]};
  auto &fileInfo = bm.getFileInfo();

  auto prevLineTokens = getLineTokens(fileInfo.before_file_path);
  auto curLineTokens = getLineTokens(fileInfo.after_file_path);

  llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> prevDiagIDs(
      new clang::DiagnosticIDs());
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> prevDiagOpts(
      new clang::DiagnosticOptions());
  clang::IgnoringDiagConsumer *prevCustomDiagnosticConsumer =
      new clang::IgnoringDiagConsumer();
  clang::DiagnosticsEngine prevDiagnostics(prevDiagIDs, prevDiagOpts,
                                           prevCustomDiagnosticConsumer);

  clang::LangOptions prevLangOpts;
  std::shared_ptr<clang::TargetOptions> prevTo =
      std::make_shared<clang::TargetOptions>();
  prevTo->Triple = llvm::sys::getDefaultTargetTriple();

  clang::TargetInfo *prevPti =
      clang::TargetInfo::CreateTargetInfo(prevDiagnostics, prevTo);
  clang::FileSystemOptions prevFsopts;
  clang::FileManager prevFileManager(prevFsopts);
  clang::SourceManager prevSourceManager(prevDiagnostics, prevFileManager);
  std::shared_ptr<clang::HeaderSearchOptions> prevHsopts =
      std::make_shared<clang::HeaderSearchOptions>();

  clang::HeaderSearch prevHeaderSearch(prevHsopts, prevSourceManager,
                                       prevDiagnostics, prevLangOpts, prevPti);

  std::shared_ptr<clang::PreprocessorOptions> prevPpopts =
      std::make_shared<clang::PreprocessorOptions>();
  clang::TrivialModuleLoader prevTrivialModuleLoader;

  clang::Preprocessor prevPreprocessor(
      prevPpopts, prevDiagnostics, prevLangOpts, prevSourceManager,
      prevHeaderSearch, prevTrivialModuleLoader);

  prevPreprocessor.Initialize(*prevPti);
  clang::FileEntryRef &prevFileEntryRef =
      prevFileManager.getFileRef(fileInfo.before_file_path).get();
  prevSourceManager.setMainFileID(prevSourceManager.createFileID(
      prevFileEntryRef, clang::SourceLocation{}, clang::SrcMgr::C_User));

  clang::IdentifierTable prevIdents;
  clang::SelectorTable prevSels;
  clang::Builtin::Context prevBuiltins;
  clang::ASTContext prevAstContext(prevLangOpts, prevSourceManager, prevIdents,
                                   prevSels, prevBuiltins,
                                   clang::TranslationUnitKind::TU_Prefix);
  prevAstContext.InitBuiltinTypes(*prevPti);

  std::shared_ptr<CustomConsumer> prevASTConsumer =
      std::make_shared<CustomConsumer>(&prevAstContext);
  std::string prevCodeCompletionStr;
  llvm::raw_string_ostream prevStream(prevCodeCompletionStr);
  clang::PrintingCodeCompleteConsumer prevPrintingCodeCompleteConsumer(
      clang::CodeCompleteOptions{}, prevStream);
  prevASTConsumer->Initialize(prevAstContext);
  prevCustomDiagnosticConsumer->BeginSourceFile(prevLangOpts,
                                                &prevPreprocessor);

  clang::Sema *prevS = new clang::Sema(
      prevPreprocessor, prevAstContext, *prevASTConsumer,
      clang::TranslationUnitKind::TU_Prefix, &prevPrintingCodeCompleteConsumer);
  MySemaHandler prevMySemaHandler(prevS);
  prevS->addExternalSource(&prevMySemaHandler);
  clang::ParseAST(*prevS, false, false);


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
  std::map<int, std::vector<int>> finalLines;
  int declIndex = 0;

  std::vector<pathInfo> allPaths;

  for (auto &declInfo : fileInfo.all_decl_info) {
    auto &prevDeclInfo = declInfo[0];
    auto &curDeclInfo = declInfo[1];
    auto prevCfgPtr = prevASTConsumer->getCfgByName(prevDeclInfo.declName);
    auto conCfgPtr = curASTConsumer->getCfgByName(curDeclInfo.declName);
    BlockInfoGenerator preBlockInfoGen(prevDeclInfo.modLines, prevCfgPtr,
                                       &prevSourceManager, &prevAstContext);
    BlockInfoGenerator curBlockInfoGen(curDeclInfo.modLines, conCfgPtr,
                                       &curSourceManager, &curAstContext);
    int N = 3;
    bm.initialize(&preBlockInfoGen, &curBlockInfoGen);
    PathGenerator curPathGenerator(N, &curBlockInfoGen, &bm, false);
    PathGenerator prePathGenerator(N, &preBlockInfoGen, &bm, true);
    ExecutionEngine preExecutionEngine(&prevAstContext, &prevLangOpts,
                                       &prevSourceManager, &bm,
                                       &preBlockInfoGen, true, prevLineTokens);
    ExecutionEngine curExecutionEngine(&curAstContext, &curLangOpts,
                                       &curSourceManager, &bm, &curBlockInfoGen,
                                       false, curLineTokens);
    PathComparator pathComparator(preBlockInfoGen, curBlockInfoGen,
                                  prePathGenerator, curPathGenerator,
                                  preExecutionEngine, curExecutionEngine, bm);

    auto allRetPaths = pathComparator.getAllInfo();
    for (auto retPath : allRetPaths) {
      pathInfo curPath;
      curPath.funcDeclName = prevDeclInfo.declName;
      curPath.patchFileName = fileInfo.patch_fileName;
      if (retPath.rawPath.empty()) {
        continue;
      }
      for (auto l : retPath.rawPath) {
        curPath.curPath.push_back(l);
        auto linfo = bm.getLineInfo(l, true);
        curPath.cids.push_back(linfo->cid);
        if (retPath.isCond) {
          curPath.isCond = true;
        }
      }
      std::sort(curPath.curPath.begin(), curPath.curPath.end());
      if (curPath.isCond) {
        allPaths.clear();
        allPaths.push_back(curPath);
        break;
      } else {
        allPaths.push_back(curPath);
      }
      declIndex++;
      bool isCond = false;
      for (auto &p : allPaths) {
        if (p.isCond) {
          isCond = true;
          break;
        }
      }
      if (isCond) {
        break;
      }
    }
  }
  std::unordered_set<std::string> cids;
  std::string str;
  llvm::raw_string_ostream stream(str);
  llvm::json::OStream J(stream);


  J.array([&] {
    for (auto &path : allPaths) {
      if (path.curPath.empty()) {
        continue;
      }
      J.object([&] {
        J.attribute("patch_file_name", path.patchFileName);
        J.attribute("func_decl_name", path.funcDeclName);
        J.attribute("is_cond", path.isCond);
        J.attributeArray("line_numbers", [&] {
          for (auto line : path.curPath) {
            J.value(line);
          }
        });
        J.attributeArray("line_cids", [&] {
          for (auto cid : path.cids) {
            J.value(cid);
          }
        });
        J.attributeArray("line_strs", [&] {
          for (auto line : path.curPath) {
            auto linfo = bm.getLineInfo(line, true);
            J.value(linfo->lineStr);
          }
        });
      });
    }
  });

  std::fstream f;
  f.open(resultPath, std::ios::out);
  f << str;
  f.close();
  return 0;
}
