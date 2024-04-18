import os
import subprocess
from tqdm import tqdm
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

def remove_whitespace(line_str):
    return ''.join(line_str.strip().split())

def compute_line_ratio(line_str1, line_str2):
    l1 = remove_whitespace(line_str1)
    l2 = remove_whitespace(line_str2)
    return Levenshtein.ratio(l1, l2)

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

def get_commits(repo,cid,patch_file_name,linenos):
    cids = []
    for entry in repo.blame_incremental(rev=cid,file=patch_file_name,L=parse_line_ranges(linenos)):
        cids.append(entry.commit.hexsha) 
    return cids

def get_file_content(repo,cid,file_path):
    try:
        source_file_content = repo.git.show(f'{cid}:{file_path}')
        return source_file_content
    except:
        return ""


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

def my_compare(i1,i2):
    if i1['datetime']<i2['datetime']:
        return -1
    elif i1['datetime']>i2['datetime']:
        return 1
    else:
        return 0

def get_bic_cids_cands(repo,cid,path_result):
    
    line_number_set = set()
    for result in path_result:
        for line_number in result['line_numbers']:
            line_number_set.add(line_number)
    
    cid_infos = []
    # print(line_number_set)
    for line_number in line_number_set:
        cur_cid = get_commits(repo,f"{cid}^1",result['patch_file_name'],[line_number])[0]
        cur_datetime = repo.commit(cur_cid).committed_datetime
        cid_infos.append({'cid':cur_cid,'datetime':cur_datetime})

    cid_infos.sort(key=functools.cmp_to_key(my_compare))

    cids = []
    cid_set = set()
    for cid_info in cid_infos:
        if cid_info['cid'] in cid_set:
            continue
        cids.append(cid_info['cid'])
        cid_set.add(cid_info['cid'])
    
    return cids

def get_line_infos(line_results,decl_name):
    decl_infos = line_results['DeclInfos']
    for decl_info in decl_infos:
        if decl_info['decl_name'] == decl_name:
            return decl_info['line_info']

    return None

def match_path(repo,cid,path_results):
    if len(path_results) == 0:
        return False
    
    patch_file_name = path_results[0]['patch_file_name']
    source_file_content = get_file_content(repo,cid,patch_file_name)
    if len(source_file_content) == 0:
        return False
    
    with open(TMP_FILE_PATH,'w') as f:
        f.write(source_file_content)

    cmd = f'{GEN_LINE_PATH} {TMP_FILE_PATH}'
    _ = subprocess.check_output(cmd, shell=True,timeout=10).decode("utf-8", errors="ignore")

    line_results = []
    with open(LINE_RESULT) as f:
        line_results = json.load(f)
    
    flag = False
    for path in path_results:
        if len(path['line_strs'])>=2:
            line_infos = get_line_infos(line_results,path['func_decl_name'])
            if line_infos!=None:
                find_cnt = 0
                for line_str in path['line_strs']:
                    for line_info in line_infos:
                        ratio = compute_line_ratio(line_str,line_info['line_str'])
                        
                        if ratio >= 0.75:
                            find_cnt = find_cnt + 1
                            break
              
                if find_cnt >= len(path['line_strs']):
                    flag = True
                    break
    return flag

def get_bic_cids(repo,pydriller_repo,cid):
    process_cid(repo,pydriller_repo,cid,SAVE_DIR)
    cid_path = os.path.join(SAVE_DIR,cid)

    test_files = []
    for f_name in os.listdir(cid_path):
        if '.json' in f_name and f_name!='result.json' and f_name!='path_result.json' and f_name!='path_result1.json' and ('result' not in f_name):
            test_files.append(f_name)

    file_paths = {}
    for test_file in test_files:
        test_file_path = os.path.join(cid_path,test_file)
        cmd = f'{GEN_RESULT_PATH} {test_file_path}'
        _ = subprocess.check_output(cmd, shell=True,timeout=10).decode("utf-8", errors="ignore")
        with open(test_file_path) as f:
            test_file_info = json.load(f)
            with open(PATH_RESULT) as f1:
                file_paths[test_file_info['patch_fileName']] = json.load(f1)
    
    final_cid_cands = []
    for patch_file,path_results in file_paths.items():
        cid_cands = get_bic_cids_cands(repo,cid,path_results)
        final_cid_cands.extend(cid_cands)
    final_cid_cands = list(set(final_cid_cands))
    if len(final_cid_cands) == 1:
        return final_cid_cands[0]

    final_cid_cands = []
    for patch_file,path_results in file_paths.items():
        cid_cands = get_bic_cids_cands(repo,cid,path_results)
        flag = False
        for cid_cand in cid_cands:
    
            if match_path(repo,cid_cand,path_results):
                flag = True
 
                final_cid_cands.append(cid_cand)
                break
        if not flag and len(cid_cands)>0:
            final_cid_cands.append(cid_cands[-1])
    
    final_cid_infos = []
    for cid_cand in final_cid_cands:
        final_cid_infos.append({'cid':cid_cand,'datetime':repo.commit(cid_cand).committed_datetime})
    
    final_cid_infos.sort(key=functools.cmp_to_key(my_compare))
    if len(final_cid_infos)>0:
        return final_cid_infos[-1]['cid']

repo = Repo(REPO_DIR)
pydriller_repo = PyDrillerGitRepo(REPO_DIR)

dataset1_cid_info = []
with open('./dataset/dataset1_cid_info.json') as f:
    dataset1_cid_info = json.load(f)
find_cnt = 0
true_cnt = 0
find_true_cnt = 0
for cid_info in dataset1_cid_info:
    fix_cid = cid_info['fix_commit_hash']
    induce_cids = cid_info['bug_commit_hash']
    try:
        bic_cid = get_bic_cids(repo,pydriller_repo,fix_cid)
        if bic_cid in induce_cids:
            find_true_cnt = find_true_cnt + 1
        find_cnt = find_cnt + 1
        true_cnt = true_cnt + len(induce_cids)
    except:
        pass
    with open('results_for_dataset1.txt','w') as f:
        f.write(f'find_true_cnt:{find_true_cnt}\n')
        f.write(f'find_cnt:{find_cnt}\n')
        f.write(f'true_cnt:{true_cnt}\n')


dataset2_cid_info = []
with open('./dataset/dataset2_cid_info.json') as f:
    dataset2_cid_info = json.load(f)
find_cnt = 0
true_cnt = 0
find_true_cnt = 0
for cid_info in dataset2_cid_info:
    fix_cid = cid_info['fix_commit_hash']
    induce_cids = cid_info['bug_commit_hash']
    try:
        bic_cid = get_bic_cids(repo,pydriller_repo,fix_cid)
        if bic_cid in induce_cids:
            find_true_cnt = find_true_cnt + 1
        find_cnt = find_cnt + 1
        true_cnt = true_cnt + len(induce_cids)
    except:
        pass
    with open('results_for_dataset2.txt','w') as f:
        f.write(f'find_true_cnt:{find_true_cnt}\n')
        f.write(f'find_cnt:{find_cnt}\n')
        f.write(f'true_cnt:{true_cnt}\n')
