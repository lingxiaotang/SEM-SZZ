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
struct InputInfo {
  std::string cidPath;
  std::string dirPath;
  std::string filePath;
  std::string fullFileName;
  std::string simpleFileName;
  std::vector<std::string> headers;
  std::vector<int> modLines;
  bool isPrev;
};
struct LineInfo {
  int lineno;
  std::string lineStr;
  bool isMod;
  std::string cid;
};

struct DeclInfo {
  bool isFuncDecl;
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

std::string getPchName(std::string simpleFileName) {
  return simpleFileName.replace(simpleFileName.find(".c"), 2, ".pch");
}

std::string getJsonName(std::string simpleFileName) {
  return simpleFileName.replace(simpleFileName.find(".c"), 2, ".json");
}

class PatchInfoASTConsumer : public clang::SemaConsumer {
  std::vector<int> *modLines;
  std::vector<std::string> modDecls;
  std::unordered_map<std::string, size_t> strToDecl;
  std::unordered_map<std::string, std::vector<int>> strToModLines;
  FileInfo finfo;
  std::string fullFileName;
  std::string fullFilePath;
  clang::SourceManager *sourceManager;

public:
  PatchInfoASTConsumer(std::vector<int> &ml, std::string fullName,
                       std::string fullPath, clang::SourceManager *sm)
      : modLines(&ml), fullFileName(fullName), fullFilePath(fullPath),
        sourceManager(sm) {
    finfo.fileName = fullName;
  }

  virtual ~PatchInfoASTConsumer() {}
  virtual bool HandleTopLevelDecl(clang::DeclGroupRef decls) override {
    clang::DeclGroupRef::iterator it;
    for (it = decls.begin(); it != decls.end(); it++) {
      if (clang::FunctionDecl *fd = llvm::dyn_cast<clang::FunctionDecl>(*it)) {
        int begLine = getLineInfo(sourceManager, fd->getBeginLoc()).second;
        int endLine = getLineInfo(sourceManager, fd->getEndLoc()).second;
        bool isMod = false;
        for (int ml : *modLines) {
          if (begLine <= ml && endLine >= ml) {
            isMod = true;
            modDecls.push_back(fd->getNameAsString());
            break;
          }
        }
        DeclInfo dinfo;
        dinfo.declName = fd->getNameAsString();
        dinfo.isFuncDecl = true;
        std::vector<LineInfo> linfos;

        if (*(sourceManager->getCharacterData(fd->getEndLoc()) + 1) == ';') {
          continue;
        }
        genLines(sourceManager->getCharacterData(fd->getBeginLoc()), begLine,
                 endLine, linfos, isMod, fd->getNameAsString());
        dinfo.lines.insert(dinfo.lines.cend(), linfos.begin(), linfos.end());
        finfo.decls.push_back(dinfo);
        strToDecl[fd->getNameAsString()] = finfo.decls.size() - 1;
      } else if (clang::RecordDecl *rd =
                     llvm::dyn_cast<clang::RecordDecl>(*it)) {
        int begLine = getLineInfo(sourceManager, rd->getBeginLoc()).second;
        int endLine = getLineInfo(sourceManager, rd->getEndLoc()).second;
        bool isMod = false;
        for (int ml : *modLines) {
          if (begLine <= ml && endLine >= ml) {
            isMod = true;
            modDecls.push_back(rd->getNameAsString());
            break;
          }
        }
        DeclInfo dinfo;
        dinfo.declName = rd->getNameAsString();
        dinfo.isFuncDecl = false;
        std::vector<LineInfo> linfos;
        genLines(sourceManager->getCharacterData(rd->getBeginLoc()), begLine,
                 endLine, linfos, isMod, rd->getNameAsString());
        dinfo.lines.insert(dinfo.lines.cend(), linfos.begin(), linfos.end());
        finfo.decls.push_back(dinfo);
        strToDecl[rd->getNameAsString()] = finfo.decls.size() - 1;
      } else if (clang::TagDecl *td = llvm::dyn_cast<clang::TagDecl>(*it)) {
        int begLine = getLineInfo(sourceManager, td->getBeginLoc()).second;
        int endLine = getLineInfo(sourceManager, td->getEndLoc()).second;
        bool isMod = false;
        for (int ml : *modLines) {
          if (begLine <= ml && endLine >= ml) {
            isMod = true;
            modDecls.push_back(td->getNameAsString());
            break;
          }
        }
        DeclInfo dinfo;
        dinfo.isFuncDecl = false;
        dinfo.declName = td->getNameAsString();
        std::vector<LineInfo> linfos;
        genLines(sourceManager->getCharacterData(td->getBeginLoc()), begLine,
                 endLine, linfos, isMod, td->getNameAsString());
        dinfo.lines.insert(dinfo.lines.cend(), linfos.begin(), linfos.end());
        finfo.decls.push_back(dinfo);
        strToDecl[td->getNameAsString()] = finfo.decls.size() - 1;
      } else if (clang::ValueDecl *vd = llvm::dyn_cast<clang::ValueDecl>(*it)) {
        int begLine = getLineInfo(sourceManager, vd->getBeginLoc()).second;
        int endLine = getLineInfo(sourceManager, vd->getEndLoc()).second;
        bool isMod = false;
        for (int ml : *modLines) {
          if (begLine <= ml && endLine >= ml) {
            isMod = true;
            modDecls.push_back(vd->getNameAsString());
            break;
          }
        }
        DeclInfo dinfo;
        dinfo.isFuncDecl = false;
        dinfo.declName = vd->getNameAsString();
        std::vector<LineInfo> linfos;
        genLines(sourceManager->getCharacterData(vd->getBeginLoc()), begLine,
                 endLine, linfos, isMod, vd->getNameAsString());
        dinfo.lines.insert(dinfo.lines.cend(), linfos.begin(), linfos.end());
        finfo.decls.push_back(dinfo);
        strToDecl[vd->getNameAsString()] = finfo.decls.size() - 1;
      }
    }
    return true;
  };
  void genLines(const char *beg, int begLine, int endLine,
                std::vector<LineInfo> &linfos, bool checkMod,
                std::string decl) {
    int curLine = begLine;
    const char *lineBeg = beg;
    strToModLines[decl];

    while (curLine <= endLine) {
      const char *ptr = lineBeg;
      for (; *ptr != '\n'; ptr++)
        ;
      LineInfo linfo;
      linfo.lineStr = std::string(lineBeg, ptr);
      linfo.lineno = curLine;
      linfo.isMod = false;
      if (checkMod) {
        auto iter = std::find(modLines->begin(), modLines->end(), linfo.lineno);
        if (iter != modLines->end()) {
          linfo.isMod = true;
          strToModLines[decl].push_back(linfo.lineno);
        }
      }
      ptr++;
      lineBeg = ptr;
      linfos.push_back(linfo);
      curLine++;
    }
  }

  FileInfo &getFileInfo() { return finfo; }
  std::vector<std::string> &getModDecls() { return modDecls; }
  DeclInfo *getDeclInfo(std::string declStr) {
    if (strToDecl.find(declStr) != strToDecl.end()) {
      return &finfo.decls[strToDecl[declStr]];
    }
    return nullptr;
  }

  std::vector<int> getModLines(std::string declName) {
    return strToModLines[declName];
  }
};

std::string toJson(std::shared_ptr<PatchInfoASTConsumer> consumerPtr,
                   InputInfo inputInfo) {
  std::string str;
  llvm::raw_string_ostream stream(str);
  llvm::json::OStream J(stream);

  J.object([&] {
    J.attribute("patch_fileName", inputInfo.fullFileName);
    J.attribute("simple_fileName", inputInfo.simpleFileName);

    J.attribute("file_path", inputInfo.filePath);
    J.attribute("dir_path", inputInfo.dirPath);
    J.attribute("pch_path", getPchName(inputInfo.filePath));

    J.attributeArray("mod_decls", [&] {
      for (auto &declName : consumerPtr->getModDecls()) {
        J.value(declName);
      }
    });

    J.attributeArray("DeclInfos", [&] {
      for (auto &decl : consumerPtr->getFileInfo().decls) {
        J.object([&] {
          J.attribute("decl_name", decl.declName);
          J.attribute("is_func", decl.isFuncDecl);
          J.attributeArray("mod_lines", [&] {
            for (auto modLines : consumerPtr->getModLines(decl.declName)) {
              J.value(modLines);
            }
          });
          J.attributeArray("line_info", [&] {
            for (auto &lineInfo : decl.lines) {
              J.object([&] {
                J.attribute("lineno", lineInfo.lineno);
                J.attribute("line_str", lineInfo.lineStr);
                J.attribute("is_mod", lineInfo.isMod);
                J.attribute("cid", lineInfo.cid);
              });
            }
          });
        });
      }
    });
  });
  return str;
}

void genPch(std::string fullFilePath, std::string simpleFileName,
            std::string dirPath, std::vector<std::string> includeDirs) {

  std::string pchName = getPchName(simpleFileName);
  std::string pchFullPath = dirPath + "/" + pchName;
  std::ifstream pf(pchFullPath);
  if (pf.good()) {
    return;
  }

  llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs(
      new clang::DiagnosticIDs());
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOpts(
      new clang::DiagnosticOptions());
  clang::IgnoringDiagConsumer *customDiagnosticConsumer =
      new clang::IgnoringDiagConsumer();
  clang::DiagnosticsEngine diagnostics(diagIDs, diagOpts,
                                       customDiagnosticConsumer);

  clang::LangOptions langOpts;
  std::shared_ptr<clang::TargetOptions> to =
      std::make_shared<clang::TargetOptions>();
  to->Triple = llvm::sys::getDefaultTargetTriple();

  clang::TargetInfo *pti = clang::TargetInfo::CreateTargetInfo(diagnostics, to);

  clang::FileSystemOptions fsopts;
  clang::FileManager fileManager(fsopts);
  clang::SourceManager sourceManager(diagnostics, fileManager);
  std::shared_ptr<clang::HeaderSearchOptions> hsopts =
      std::make_shared<clang::HeaderSearchOptions>();

  clang::HeaderSearch headerSearch(hsopts, sourceManager, diagnostics, langOpts,
                                   pti);

  for (auto includeDir : includeDirs) {
    auto expectedDirectoryRef = fileManager.getDirectoryRef(includeDir);
    clang::DirectoryLookup directoryLookup(
        expectedDirectoryRef.get(), clang::SrcMgr::CharacteristicKind::C_User,
        false);
    headerSearch.AddSearchPath(directoryLookup, false);
  }

  std::shared_ptr<clang::PreprocessorOptions> ppopts =
      std::make_shared<clang::PreprocessorOptions>();
  clang::TrivialModuleLoader trivialModuleLoader;
  clang::Preprocessor preprocessor(ppopts, diagnostics, langOpts, sourceManager,
                                   headerSearch, trivialModuleLoader);

  preprocessor.Initialize(*pti);
  clang::FileEntryRef &fileEntryRef =
      fileManager.getFileRef(fullFilePath).get();
  sourceManager.setMainFileID(sourceManager.createFileID(
      fileEntryRef, clang::SourceLocation{}, clang::SrcMgr::C_User));
  customDiagnosticConsumer->BeginSourceFile(langOpts, &preprocessor);

  std::string outPut;
  llvm::raw_string_ostream sstream(outPut);
  clang::PreprocessorOutputOptions Opts;
  Opts.ShowCPP = 1;
  Opts.ShowLineMarkers = 1;
  Opts.ShowComments = 1;
  Opts.ShowMacroComments = 1;
  Opts.ShowMacros = 1;
  Opts.DirectivesOnly = 1;
  DoPrintPreprocessedInput(preprocessor, &sstream, Opts);

  std::ofstream outfile;
  outfile.open(pchFullPath, std::ios::out | std::ios::trunc);
  outfile << sstream.str();
  outfile.close();
}

std::string readFile(std::string path) {
  std::ifstream inputFile(path);
  if (!inputFile.is_open()) {
    std::cerr << "can not open the file " << path << std::endl;
    exit(1);
  }
  std::stringstream buffer;
  buffer << inputFile.rdbuf();
  inputFile.close();
  std::string fileContent = buffer.str();
  return fileContent;
}

void writeFile(std::string path, std::string content) {
  std::fstream f;
  f.open(path, std::ios::out);
  f << content;
  f.close();
}

InputInfo fromJsonInputInfo(std::string str) {

  llvm::Expected<llvm::json::Value> e = llvm::json::parse(str);

  InputInfo inputInfo;
  llvm::json::Object *o = e->getAsObject();

  assert(o != nullptr);
  std::optional<llvm::StringRef> cid_path_ref = o->getString("cid_path");
  assert(cid_path_ref.has_value());
  inputInfo.cidPath = cid_path_ref.value().str();
  
  std::optional<llvm::StringRef> dir_path_ref = o->getString("dir_path");
  assert(dir_path_ref.has_value());
  inputInfo.dirPath = dir_path_ref.value().str();
  
  std::optional<llvm::StringRef> file_path_ref = o->getString("file_path");
  assert(file_path_ref.has_value());
  inputInfo.filePath = file_path_ref.value().str();
  
  std::optional<llvm::StringRef> full_file_name_ref =
      o->getString("full_file_name");
  assert(full_file_name_ref.has_value());
  inputInfo.fullFileName = full_file_name_ref.value().str();
  
  std::optional<llvm::StringRef> simple_file_name_ref =
      o->getString("simple_file_name");
  assert(simple_file_name_ref.has_value());
  inputInfo.simpleFileName = simple_file_name_ref.value().str();
  
  auto mods_array = o->getArray("mod_lines");
  assert(mods_array);

  for (auto modsArrayIter = mods_array->begin();
       modsArrayIter != mods_array->end(); modsArrayIter++) {
    auto modLinenoOpt = modsArrayIter->getAsInteger();
    assert(modLinenoOpt.hasValue());
    inputInfo.modLines.push_back(modLinenoOpt.value());
  }

  auto headers_array = o->getArray("headers");
  assert(headers_array);

  for (auto headersIter = headers_array->begin();
       headersIter != headers_array->end(); headersIter++) {
    auto headerOpt = headersIter->getAsString();
    assert(headerOpt.hasValue());
    inputInfo.headers.push_back(headerOpt.value().str());
  }
  return inputInfo;
}

std::shared_ptr<PatchInfoASTConsumer> genFileInfo(InputInfo &inputInfo) {
  std::string pchName = getPchName(inputInfo.simpleFileName);
  genPch(inputInfo.filePath, inputInfo.simpleFileName, inputInfo.dirPath,
         inputInfo.headers);
  llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs(
      new clang::DiagnosticIDs());
  llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions> diagOpts(
      new clang::DiagnosticOptions());
  clang::IgnoringDiagConsumer *customDiagnosticConsumer =
      new clang::IgnoringDiagConsumer();
  clang::DiagnosticsEngine diagnostics(diagIDs, diagOpts,
                                       customDiagnosticConsumer);

  clang::LangOptions langOpts;
  std::shared_ptr<clang::TargetOptions> to =
      std::make_shared<clang::TargetOptions>();
  to->Triple = llvm::sys::getDefaultTargetTriple();

  clang::TargetInfo *pti = clang::TargetInfo::CreateTargetInfo(diagnostics, to);
  clang::FileSystemOptions fsopts;
  clang::FileManager fileManager(fsopts);
  clang::SourceManager sourceManager(diagnostics, fileManager);
  std::shared_ptr<clang::HeaderSearchOptions> hsopts =
      std::make_shared<clang::HeaderSearchOptions>();

  clang::HeaderSearch headerSearch(hsopts, sourceManager, diagnostics, langOpts,
                                   pti);

  std::shared_ptr<clang::PreprocessorOptions> ppopts =
      std::make_shared<clang::PreprocessorOptions>();
  clang::TrivialModuleLoader trivialModuleLoader;
  clang::Preprocessor preprocessor(ppopts, diagnostics, langOpts, sourceManager,
                                   headerSearch, trivialModuleLoader);

  preprocessor.Initialize(*pti);
  clang::FileEntryRef &fileEntryRef =
      fileManager.getFileRef(inputInfo.filePath).get();
  sourceManager.setMainFileID(sourceManager.createFileID(
      fileEntryRef, clang::SourceLocation{}, clang::SrcMgr::C_User));

  clang::IdentifierTable idents;
  clang::SelectorTable sels;
  clang::Builtin::Context builtins;
  clang::ASTContext astContext(langOpts, sourceManager, idents, sels, builtins,
                               clang::TranslationUnitKind::TU_Prefix);
  astContext.InitBuiltinTypes(*pti);
  astContext.Idents;
  std::shared_ptr<PatchInfoASTConsumer> patchInfoASTConsumer =
      std::make_shared<PatchInfoASTConsumer>(
          inputInfo.modLines, inputInfo.fullFileName, inputInfo.filePath,
          &sourceManager);
  std::string codeCompletionStr;
  llvm::raw_string_ostream stream(codeCompletionStr);
  clang::PrintingCodeCompleteConsumer printingCodeCompleteConsumer(
      clang::CodeCompleteOptions{}, stream);
  patchInfoASTConsumer->Initialize(astContext);
  customDiagnosticConsumer->BeginSourceFile(langOpts, &preprocessor);

  clang::ParseAST(preprocessor, patchInfoASTConsumer.get(), astContext, false,
                  clang::TranslationUnitKind::TU_Prefix,
                  &printingCodeCompleteConsumer, false);

  return patchInfoASTConsumer;
}

int main(int argc, char const *argv[]) {
  if (argc != 2) {
    exit(-1);
  }

  auto infoPath = argv[1];
  std::string infoStr = readFile(infoPath);
  
  InputInfo inputInfo = fromJsonInputInfo(infoStr);
 
  auto consumerPtr = genFileInfo(inputInfo);
  std::unordered_set<std::string> declSet;

  std::string jsonStr = toJson(consumerPtr, inputInfo);
  std::string jsonPath =
      inputInfo.dirPath + "/" + getJsonName(inputInfo.simpleFileName);
  writeFile(jsonPath, jsonStr);
  return 0;
}
