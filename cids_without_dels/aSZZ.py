# %%
# %%
import os
import subprocess
import json
import logging as log
import ntpath
import os
from abc import ABC, abstractmethod
from shutil import copytree
from enum import Enum
from shutil import rmtree
# from typing import List, Setgen_file_info
from tempfile import mkdtemp

from git import Commit, Repo, IndexFile
from pydriller import ModificationType, GitRepository as PyDrillerGitRepo
from settings import *

import functools
import Levenshtein
import traceback
# %%


# %%
def parse_line_ranges(modified_lines):        
    mod_line_ranges = list()
    if len(modified_lines) > 0:
        start = int(modified_lines[0])
        end = int(modified_lines[0])
        if len(modified_lines) == 1:
            return [f'{start},{end}']
        for i in range(1, len(modified_lines)):
            line = int(modified_lines[i])
            if line - end == 1:
                end = line
            else:
                mod_line_ranges.append(f'{start},{end}')
                start = line
                end = line
            if i == len(modified_lines) - 1:
                mod_line_ranges.append(f'{start},{end}')
    return mod_line_ranges

# %%
def get_commits(repo,cid,patch_file_name,linenos):
   cids = []
   for entry in repo.blame_incremental(rev=cid,file=patch_file_name,L=parse_line_ranges(linenos)):
      cids.append(entry.commit.hexsha) 
   return cids

# %%
def get_file_content(repo,cid,file_path):
    try:
        source_file_content = repo.git.show(f'{cid}:{file_path}')
        return source_file_content
    except:
        return ""

# %%
def get_mod_file_info(pydriller_repo,cid):
    mod_file_info = {}
    blame_commit = pydriller_repo.get_commit(cid)
    for mod in blame_commit.modifications:
        if mod.old_path == mod.new_path and '.c' in mod.old_path:
            if mod.old_path not in mod_file_info:
                mod_file_info[mod.old_path] = []
            for added in mod.diff_parsed['added']:
                mod_file_info[mod.old_path].append(added[0])

    return mod_file_info


def get_decl_info(decl_infos,decl_name):
    for decl_info in decl_infos:
        if decl_info['decl_name'] == decl_name:
            return  decl_info    
    return None


def gen_file_info(repo,before_info,after_info,cid):
    file_info = {}
    file_info['patch_fileName'] = before_info['patch_fileName']
    file_info['simple_fileName'] = before_info['simple_fileName']
    file_info['before_file_path'] = before_info['file_path']
    file_info['before_dir_path'] = before_info['dir_path']
    file_info['before_pch_path'] = before_info['pch_path']
    file_info['before_mod_decls'] = before_info['mod_decls']
    file_info['after_file_path'] = after_info['file_path']
    file_info['after_dir_path'] =  after_info['dir_path']
    file_info['after_pch_path'] =  after_info['pch_path']
    file_info['after_mod_decls'] = after_info['mod_decls']
    file_info['decl_infos'] = []
    mod_set = set()
    for mod_decl in before_info['mod_decls']:
        if mod_decl in mod_set:
            continue
        before_mod_info = get_decl_info(before_info['DeclInfos'],mod_decl)
        after_mod_info = get_decl_info(after_info['DeclInfos'],mod_decl)
        if after_mod_info!=None:
            mod_set.add(mod_decl)            
            # add_commits(repo,f'{cid}^1',file_info['patch_fileName'],before_mod_info)
            file_info['decl_infos'].append([before_mod_info,after_mod_info])
    for mod_decl in after_info['mod_decls']:
        if mod_decl in mod_set:
            continue
        after_mod_info = get_decl_info(after_info['DeclInfos'],mod_decl)
        before_mod_info = get_decl_info(before_info['DeclInfos'],mod_decl)
        if before_mod_info!=None:
            mod_set.add(mod_decl)
            # add_commits(repo,f'{cid}^1',file_info['patch_fileName'],before_mod_info)
            file_info['decl_infos'].append([before_mod_info,after_mod_info])    
    return file_info

def process_cid(repo,pydriller_repo,cid,save_dir):
    for mod_file,mod_ls in get_mod_file_info(pydriller_repo,cid).items():
        full_name = mod_file
        simple_name = mod_file.split('/')[-1]
        info_after = {}
        info_after['dir_path'] = os.path.join(save_dir,cid,'after')
        info_after['file_path'] = os.path.join(info_after['dir_path'],simple_name)
        info_after['full_file_name'] = full_name
        info_after['simple_file_name'] = simple_name        
        info_after['mod_lines'] = mod_ls
        info_after['cid_path'] = os.path.join(save_dir,cid)
        info_after['headers'] = []
        if not os.path.exists(info_after['dir_path']):
            os.makedirs(info_after['dir_path'])
        prev_file_content = get_file_content(repo,cid,mod_file)
        with open(info_after['file_path'],'w') as f:
            f.write(prev_file_content)
        tmp_path = os.path.join(save_dir,'tmp.json')
        with open(tmp_path,'w') as f:
            json.dump(info_after,f)
        cmd = f'{GEN_INFO_PATH} {tmp_path}'
        _ = subprocess.check_output(cmd, shell=True).decode("utf-8", errors="ignore")
        info_before = {}
        info_before['dir_path'] = os.path.join(save_dir,cid,'before')
        info_before['file_path'] = os.path.join(info_before['dir_path'],simple_name)
        info_before['full_file_name'] = full_name
        info_before['simple_file_name'] = simple_name        
        info_before['mod_lines'] = []
        info_before['cid_path'] = os.path.join(save_dir,cid)
        info_before['headers'] = []
        if not os.path.exists(info_before['dir_path']):
            os.makedirs(info_before['dir_path'])
        con_file_content = get_file_content(repo,f'{cid}^1',mod_file)
        with open(info_before['file_path'],'w') as f:
            f.write(con_file_content)
        with open(tmp_path,'w') as f:
            json.dump(info_before,f)
        cmd = f'{GEN_INFO_PATH} {tmp_path}'
        _ = subprocess.check_output(cmd, shell=True).decode("utf-8", errors="ignore")
        file_info_before = {}
        file_info_after = {}
        file_info_before_path = os.path.join(info_before['dir_path'],info_before['simple_file_name'].replace('.c','.json'))
        file_info_after_path = os.path.join(info_after['dir_path'],info_after['simple_file_name'].replace('.c','.json'))
        with open(file_info_before_path,'r') as f:
            file_info_before = json.load(f)
        with open(file_info_after_path,'r') as f:
            file_info_after = json.load(f)
        file_info = gen_file_info(repo,file_info_before,file_info_after,cid)
        file_info_path = os.path.join(info_before['cid_path'],info_before['simple_file_name'].replace('.c','.json'))
        with open(file_info_path,'w') as f:
            json.dump(file_info,f)


def get_bic_cids(repo,pydriller_repo,cid):
    cid_path = os.path.join(SAVE_DIR,cid)
    if not os.path.exists(cid_path):
        process_cid(repo,pydriller_repo,cid,SAVE_DIR) 
    bic_cids = set()
    for f_name in os.listdir(cid_path):
        if '.json' in f_name and f_name!='result.json':
            test_file_path = os.path.join(cid_path,f_name)
            
            cmd = f'{ASZZ_BIN_PATH} {test_file_path}'
            _ = subprocess.check_output(cmd, shell=True,timeout=10).decode("utf-8", errors="ignore")
            lines = []
            with open(ASZZ_RESULT) as f:
                lines = json.load(f)
            patch_file_name = ''
            with open(test_file_path) as f:
                file_info = json.load(f)
                patch_file_name = file_info['patch_fileName']
            cur_bics = get_commits(repo,cid,patch_file_name,lines)
            for bic in cur_bics:
                bic_cids.add(bic)
    
    return list(bic_cids)


repo = Repo(REPO_DIR)
pydriller_repo = PyDrillerGitRepo(REPO_DIR)
dataset1_cid_info = []
with open('.dataset/dataset1_cid_info.json') as f:
    dataset1_cid_info = json.load(f)

# %%
find_cnt = 0
true_cnt = 0
find_true_cnt = 0
for cid_info in dataset1_cid_info:
    fix_cid = cid_info['fix_commit_hash']
    induce_cids = cid_info['bug_commit_hash']
    try:
        find_cids = get_bic_cids(repo,pydriller_repo,fix_cid)
        print(f'find_cids:{find_cids}')
        find_cnt = find_cnt + len(find_cids)
        true_cnt = true_cnt + len(induce_cids)
        cnt = 0
        for cid1 in find_cids:
            if cid1 in induce_cids:
                cnt = cnt + 1
        find_true_cnt = find_true_cnt + cnt
    except:
        pass
    with open(f'dataset1_aSZZResult.txt','w') as f:
        f.write(f'find_true_cnt:{find_true_cnt}\n')
        f.write(f'find_cnt:{find_cnt}\n')
        f.write(f'true_cnt:{true_cnt}\n')


dataset2_cid_info = []
with open('.dataset/dataset2_cid_info.json') as f:
    dataset2_cid_info = json.load(f)
find_cnt = 0
true_cnt = 0
find_true_cnt = 0
for cid_info in dataset2_cid_info:
    fix_cid = cid_info['fix_commit_hash']
    induce_cids = cid_info['bug_commit_hash']
    try:
        find_cids = get_bic_cids(repo,pydriller_repo,fix_cid)
        print(f'find_cids:{find_cids}')
        find_cnt = find_cnt + len(find_cids)
        true_cnt = true_cnt + len(induce_cids)
        cnt = 0
        for cid1 in find_cids:
            if cid1 in induce_cids:
                cnt = cnt + 1
        find_true_cnt = find_true_cnt + cnt
    except:
        pass
    with open(f'dataset2_aSZZResult.txt','w') as f:
        f.write(f'find_true_cnt:{find_true_cnt}\n')
        f.write(f'find_cnt:{find_cnt}\n')
        f.write(f'true_cnt:{true_cnt}\n')