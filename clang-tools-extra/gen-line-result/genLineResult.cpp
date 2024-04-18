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
#include "clang/Parse/ParseAST.h"
#include "clang/Sema/CodeCompleteConsumer.h"
#include "clang/Sema/SemaConsumer.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/JSON.h"
#include "llvm/TargetParser/Host.h"
#include <algorithm>
#include <fstream>
#include <iostream>
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
};

struct DeclInfo {
  std::string declName;
  std::vector<LineInfo> lines;
};

struct FileInfo {
  std::vector<DeclInfo> decls;
  std::string fileName;
};

std::pair<std::string, int> getLineInfo(clang::SourceManager *sm,
                                        clang::SourceLocation sl) {
  clang::PresumedLoc pLoc = sm->getPresumedLoc(sl);
  assert(pLoc.isValid());
  return std::make_pair<std::string, int>(pLoc.getFilename(), pLoc.getLine());
}

class PatchInfoASTConsumer : public clang::SemaConsumer {
  FileInfo finfo;
  std::string fileName;
  std::string fullFilePath;
  clang::SourceManager *sourceManager;

public:
  PatchInfoASTConsumer(std::string fn, clang::SourceManager *sm)
      : fileName(fn), sourceManager(sm) {}

  virtual ~PatchInfoASTConsumer() {}
  virtual bool HandleTopLevelDecl(clang::DeclGroupRef decls) override {
    clang::DeclGroupRef::iterator it;
    for (it = decls.begin(); it != decls.end(); it++) {
      if (clang::FunctionDecl *fd = llvm::dyn_cast<clang::FunctionDecl>(*it)) {
        int begLine = getLineInfo(sourceManager, fd->getBeginLoc()).second;
        int endLine = getLineInfo(sourceManager, fd->getEndLoc()).second;
        std::vector<LineInfo> lineInfos;
        genLines(sourceManager->getCharacterData(fd->getBeginLoc()), begLine,
                 endLine, lineInfos);
        DeclInfo dinfo;
        dinfo.declName = fd->getNameAsString();
        dinfo.lines.insert(dinfo.lines.end(), lineInfos.begin(),
                           lineInfos.end());
        finfo.decls.push_back(dinfo);
      }
    }
    return true;
  }

  void genLines(const char *beg, int begLine, int endLine,
                std::vector<LineInfo> &linfos) {
    int curLine = begLine;
    const char *lineBeg = beg;

    while (curLine <= endLine) {
      const char *ptr = lineBeg;
      for (; *ptr != '\n'; ptr++)
        ;
      LineInfo linfo;
      linfo.lineStr = std::string(lineBeg, ptr);
      linfo.lineno = curLine;

      ptr++;
      lineBeg = ptr;
      linfos.push_back(linfo);
      curLine++;
    }
  }
  FileInfo &getFileInfo() { return finfo; }
};

std::string toJson(std::shared_ptr<PatchInfoASTConsumer> consumerPtr) {
  std::string str;
  llvm::raw_string_ostream stream(str);
  llvm::json::OStream J(stream);

  J.object([&] {
    J.attributeArray("DeclInfos", [&] {
      for (auto &decl : consumerPtr->getFileInfo().decls) {
        J.object([&] {
          J.attribute("decl_name", decl.declName);
          J.attributeArray("line_info", [&] {
            for (auto &lineInfo : decl.lines) {
              J.object([&] {
                J.attribute("lineno", lineInfo.lineno);
                J.attribute("line_str", lineInfo.lineStr);
              });
            }
          });
        });
      }
    });
  });
  return str;
}

void writeFile(std::string path, std::string content) {
  std::fstream f;
  f.open(path, std::ios::out);
  f << content;
  f.close();
}
// enter your tmpFilePath here, the variable should be the same with the LINE_RESULT variable in settings.py in cids_without_dels directory 
const std::string tmpFilePath =
      "";

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    exit(-1);
  }

  std::string filePath = std::string(argv[1]);

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

  std::shared_ptr<PatchInfoASTConsumer> prevASTConsumer =
      std::make_shared<PatchInfoASTConsumer>(filePath, &prevSourceManager);
  std::string prevCodeCompletionStr;
  llvm::raw_string_ostream prevStream(prevCodeCompletionStr);
  clang::PrintingCodeCompleteConsumer prevPrintingCodeCompleteConsumer(
      clang::CodeCompleteOptions{}, prevStream);
  prevASTConsumer->Initialize(prevAstContext);
  prevCustomDiagnosticConsumer->BeginSourceFile(prevLangOpts,
                                                &prevPreprocessor);

  clang::ParseAST(prevPreprocessor, prevASTConsumer.get(), prevAstContext,
                  false, clang::TranslationUnitKind::TU_Prefix,
                  &prevPrintingCodeCompleteConsumer, false);

  auto &fileInfo = prevASTConsumer->getFileInfo();
  for (auto &declInfo : fileInfo.decls) {
    llvm::outs() << declInfo.declName << "\n";
  }
  writeFile(tmpFilePath, toJson(prevASTConsumer));
  llvm::outs() << toJson(prevASTConsumer) << "\n";
  return 0;
}