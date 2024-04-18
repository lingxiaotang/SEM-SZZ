#include "BlockMapper.h"
#include "StateCollector.h"

std::string BlockMapper::getBlockStr(clang::CFGBlock *cfgBlock, bool isPrev) {
  BlockInfoGenerator *blockInfoGen;
  if (isPrev) {
    blockInfoGen = prevGen;
  } else {
    blockInfoGen = conGen;
  }

  std::string str;
  for (clang::CFGBlock::const_iterator I = cfgBlock->begin(),
                                       E = cfgBlock->end();
       I != E; ++I) {
    switch ((*I).getKind()) {
    case clang::CFGElement::Kind::Statement:
      clang::CFGStmt CS = (*I).castAs<clang::CFGStmt>();
      const clang::Stmt *S = CS.getStmt();

      str.append(
          ExecutionEngine::getStmtAsString(S, blockInfoGen->getAstContext()));
    }
  }

  return str;
}

BlockMapper::BlockMapper(std::string infoFilePath_)
    : infoFilePath{infoFilePath_} {
  std::ifstream inputFile(this->infoFilePath);
  if (!inputFile.is_open()) {
    std::cerr << "can not open file: " << this->infoFilePath << std::endl;
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
  initLineInfo();
  initLinfoMap();
}

void BlockMapper::initialize(BlockInfoGenerator *prevGen_,
                             BlockInfoGenerator *conGen_) {
  prevGen = prevGen_;
  conGen = conGen_;
}

FileInfo &BlockMapper::getFileInfo() { return fileInfo; }
std::vector<int> &BlockMapper::getPrevModLines() { return prevModLines; }
std::vector<int> &BlockMapper::getCurModLines() { return conModLines; }

void BlockMapper::initLineInfo() {
  for (auto &decl_info_pair : fileInfo.all_decl_info) {
    auto &before_decl = decl_info_pair[0];
    auto &after_decl = decl_info_pair[1];

    int i = 0, j = 0;
    while (i < before_decl.lines.size() && j < after_decl.lines.size()) {
      if ((!before_decl.lines[i].isMod) && (!after_decl.lines[j].isMod)) {
        lineMap1[before_decl.lines[i].lineno] = after_decl.lines[j].lineno;
        lineMap2[after_decl.lines[j].lineno] = before_decl.lines[i].lineno;
        i++;
        j++;
      } else if ((!before_decl.lines[i].isMod) && (after_decl.lines[j].isMod)) {
        j++;
      } else if ((before_decl.lines[i].isMod) && (!after_decl.lines[j].isMod)) {
        i++;
      } else {
        i++;
        j++;
      }
    }
  }

  for (auto &decl_info_pair : fileInfo.all_decl_info) {
    auto &before_decl = decl_info_pair[0];
    auto &after_decl = decl_info_pair[1];
    for (auto &linfo : before_decl.lines) {
      if (linfo.isMod) {
        prevModLines.push_back(linfo.lineno);
      }
    }
    for (auto &linfo : after_decl.lines) {
      if (linfo.isMod) {
        conModLines.push_back(linfo.lineno);
      }
    }
  }
}

void BlockMapper::printLines(int begLineno, int endLineno, bool isPrev) {
  std::unordered_map<int, LineInfo *> *linfoMap;
  if (isPrev) {
    linfoMap = &preLinfoMap;
  } else {
    linfoMap = &curLinfoMap;
  }
  if (begLineno == -1) {
    return;
  }
  for (int i = begLineno; i <= endLineno; i++) {
    if (i == 0) {
      continue;
    }
    if (i == INT_MAX) {
      break;
    }
    auto &curLinfo = (*linfoMap)[i];
  }
}

void BlockMapper::printBlock(clang::CFGBlock *block, bool isPrev) {

  if (!block) {
    return;
  }
  BlockInfoGenerator *blockInfoGen;
  if (isPrev) {
    blockInfoGen = prevGen;
  } else {
    blockInfoGen = conGen;
  }

  for (clang::CFGBlock::const_iterator I = block->begin(), E = block->end();
       I != E; ++I) {
    switch ((*I).getKind()) {
    case clang::CFGElement::Kind::Statement:
      clang::CFGStmt CS = (*I).castAs<clang::CFGStmt>();
      const clang::Stmt *S = CS.getStmt();
    }
  }
}

void BlockMapper::initLinfoMap() {
  for (auto &declPair : fileInfo.all_decl_info) {
    auto &preDecl = declPair[0];
    auto &curDecl = declPair[1];

    for (auto &linfo : preDecl.lines) {
      preLinfoMap[linfo.lineno] = &linfo;
    }

    for (auto &linfo : curDecl.lines) {
      curLinfoMap[linfo.lineno] = &linfo;
    }
  }
}

bool BlockMapper::hasUnmodBlock(clang::CFGBlock *cfgBlock, bool isPrev) {
  if (blockMap1.find(cfgBlock) != blockMap1.end()) {
    return true;
  }

  if (blockMap2.find(cfgBlock) != blockMap2.end()) {
    return true;
  }

  BlockInfoGenerator *p1 = nullptr, *p2 = nullptr;
  std::unordered_map<int, int> *lmap = nullptr;
  if (isPrev) {
    p1 = prevGen;
    p2 = conGen;
    lmap = &lineMap1;
  } else {
    p1 = conGen;
    p2 = prevGen;
    lmap = &lineMap2;
  }

  if (cfgBlock == p1->getBegBlock() || cfgBlock == p1->getEndBlock()) {
    return true;
  }

  int begLine = p1->getBegLine(cfgBlock);
  int endLine = p1->getEndLine(cfgBlock);

  assert(lmap->find(begLine) != lmap->end());
  assert(lmap->find(endLine) != lmap->end());

  int begLine_ = (*lmap)[begLine];
  int endLine_ = (*lmap)[endLine];

  auto b1 = p2->findBlock(begLine_)[0];
  auto b2 = p2->findBlock(endLine_)[0];
  if (b1 != b2) {
    return false;
  }
  if (isPrev) {
    blockMap1[cfgBlock] = b1;
    blockMap2[b1] = cfgBlock;
  } else {
    blockMap2[cfgBlock] = b1;
    blockMap1[b1] = cfgBlock;
  }
  return true;
}

clang::CFGBlock *BlockMapper::getCorrespondBlock(clang::CFGBlock *cfgBlock,
                                                 bool isPrev) {
  if (blockMap1.find(cfgBlock) != blockMap1.end()) {
    return blockMap1[cfgBlock];
  }

  if (blockMap2.find(cfgBlock) != blockMap2.end()) {
    return blockMap2[cfgBlock];
  }

  BlockInfoGenerator *p1 = nullptr, *p2 = nullptr;
  std::unordered_map<int, int> *lmap = nullptr;
  if (isPrev) {
    p1 = prevGen;
    p2 = conGen;
    lmap = &lineMap1;
  } else {
    p1 = conGen;
    p2 = prevGen;
    lmap = &lineMap2;
  }
  if (cfgBlock == (p1->getBegBlock())) {
    return p2->getBegBlock();
  }

  if (cfgBlock == (p1->getEndBlock())) {
    return p2->getEndBlock();
  }
 
  int begLine = p1->getBegLine(cfgBlock);
  int endLine = p1->getEndLine(cfgBlock);
  assert(lmap->find(begLine) != lmap->end());
  assert(lmap->find(endLine) != lmap->end());

  if (lmap->find(begLine) == lmap->end() ||
      lmap->find(endLine) == lmap->end()) {
    return nullptr;
  }

  int begLine_ = (*lmap)[begLine];
  int endLine_ = (*lmap)[endLine];
  auto bs1 = p2->findBlock(begLine_);
  for (auto b : bs1) {
    printBlock(b, true);
  }
  auto bs2 = p2->findBlock(endLine_);
  for (auto b : bs2) {
    printBlock(b, true);
  }

  std::vector<clang::CFGBlock *> tmp;
  for (auto b1 : bs1) {
    if (std::find(bs2.begin(), bs2.end(), b1) != bs2.end()) {
      tmp.push_back(b1);
    }
  }
  bs1 = tmp;
  clang::CFGBlock *ccfgBlock = nullptr;
  if (bs1.size() > 1) {
    int cnt = 0;
    for (auto b : bs1) {
      if (getBlockStr(b, !isPrev) == getBlockStr(cfgBlock, isPrev)) {
        ccfgBlock = b;
        cnt++;
      }
    }
    if (cnt > 1) {
      return nullptr;
    }
  } else if(bs1.empty()){
    return nullptr;
  } else {
    ccfgBlock = bs1[0];
  }
  if (!ccfgBlock) {
    return nullptr;
  }
  if (isPrev) {
    blockMap1[cfgBlock] = ccfgBlock;
    blockMap2[ccfgBlock] = cfgBlock;
  } else {
    blockMap2[cfgBlock] = ccfgBlock;
    blockMap1[ccfgBlock] = cfgBlock;
  }
  return ccfgBlock;
}

int BlockMapper::preLineToCur(int lineno) {
  if (lineMap1.find(lineno) != lineMap1.end()) {
    return lineMap1[lineno];
  }
  return -1;
}
int BlockMapper::curLineToPre(int lineno) {
  if (lineMap2.find(lineno) != lineMap2.end()) {
    return lineMap2[lineno];
  }
  return -1;
}

int BlockMapper::getMaxLineno(std::vector<clang::CFGBlock *> &path,
                              bool isPrev) {
  BlockInfoGenerator *blockInfoGen;
  std::unordered_map<int, int> *lmap = nullptr;
  std::unordered_map<int, LineInfo *> *linfoMap = nullptr;

  if (isPrev) {
    blockInfoGen = prevGen;
    lmap = &lineMap1;
    linfoMap = &preLinfoMap;
  } else {
    blockInfoGen = conGen;
    lmap = &lineMap2;
    linfoMap = &curLinfoMap;
  }

  int endLine = -1;
  for (int i = path.size() - 1; i >= 0; i--) {
    auto curEndLine = blockInfoGen->getEndLine(path[i]);
    if (curEndLine != -1 && curEndLine != INT_MAX) {
      if (curEndLine > endLine) {
        endLine = curEndLine;
      }
    }
  }

  assert(endLine != -1 && endLine != INT_MAX);
  int i = endLine + 1;
  while (true) {
    if (linfoMap->find(i) == linfoMap->end()) {
      return -1;
    }
    auto linfo = (*linfoMap)[i];
    if (!linfo->isMod) {
      assert(lmap->find(i) != lmap->end());
      return (*lmap)[i];
    }
    i++;
  }
  return -1;
}

LineInfo *BlockMapper::getLineInfo(int lineno, bool isPrev) {
  std::unordered_map<int, LineInfo *> *linfoMapPtr = nullptr;
  if (isPrev) {
    linfoMapPtr = &preLinfoMap;
  } else {
    linfoMapPtr = &curLinfoMap;
  }

  if (linfoMapPtr->find(lineno) == linfoMapPtr->end()) {
    return nullptr;
  }
  return (*linfoMapPtr)[lineno];
}