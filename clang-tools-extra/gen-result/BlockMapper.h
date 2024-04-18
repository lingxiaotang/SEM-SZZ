#ifndef BLOCKMAPPER_H
#define BLOCKMAPPER_H
#include "BlockInfoGenerator.h"
#include "clang/Analysis/CFG.h"
#include "llvm/Support/JSON.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

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

class BlockMapper {
  std::string infoFilePath;
  std::vector<int> prevModLines;
  std::vector<int> conModLines;

  std::unordered_map<int, int> lineMap1;
  std::unordered_map<int, int> lineMap2;
  std::unordered_map<clang::CFGBlock *, clang::CFGBlock *> blockMap1;
  std::unordered_map<clang::CFGBlock *, clang::CFGBlock *> blockMap2;

  std::unordered_map<int, LineInfo *> preLinfoMap;
  std::unordered_map<int, LineInfo *> curLinfoMap;

  BlockInfoGenerator *prevGen;
  BlockInfoGenerator *conGen;
  FileInfo fileInfo;

  void initFileInfo();
  void initLineInfo();
  void initLinfoMap();

  std::string getBlockStr(clang::CFGBlock *cfgBlock, bool isPrev);

public:
  BlockMapper(std::string infoFilePath_);
  void initialize(BlockInfoGenerator *prevGen_, BlockInfoGenerator *conGen_);
  clang::CFGBlock *getCorrespondBlock(clang::CFGBlock *cfgBlock, bool isPrev);
  bool hasUnmodBlock(clang::CFGBlock *cfgBlock, bool isPrev);
  int preLineToCur(int lineno);
  int curLineToPre(int lineno);
  void printLines(int begLineno, int endLineno, bool isPrev);
  void printBlock(clang::CFGBlock *block, bool isPrev);
  FileInfo &getFileInfo();
  std::vector<int> &getPrevModLines();
  std::vector<int> &getCurModLines();
  int getMaxLineno(std::vector<clang::CFGBlock *> &path, bool isPrev);
  LineInfo *getLineInfo(int lineno, bool isPrev);
};

#endif