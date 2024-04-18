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
from typing import List, Set
from tempfile import mkdtemp
import git
from pydriller import ModificationType, GitRepository as PyDrillerGitRepo
import traceback
from settings import *

repo = git.Repo(REPO_DIR)

def getEarliestCid(cids,repo):
    if len(cids)==0:
        return cids
    min_date = None
    min_cid = None
    for cid in cids:
        cur_date = repo.commit(cid).committed_datetime
        if min_date == None:
            min_date = cur_date
            min_cid = cid
        elif cur_date < min_date:
            min_date = cur_date
            min_cid = cid
    
    ret_set = set()
    ret_set.add(min_cid)
    return min_cid

cids = os.listdir('./result/3')

find_cnt = 0
find_true_cnt = 0
true_cnt = 0
for cid in cids:
    cid_path = os.path.join('./result/3',cid)
    result_path = os.path.join(cid_path,'path_result.json')
    result = {}
    with open(result_path) as f:
        result = json.load(f)
    is_cond = False
    for _,path_infos in result.items():
        for path_info in path_infos:
            if path_info['is_cond']:
                is_cond = True

    result_path = os.path.join(cid_path,'result.json')
    with open(result_path) as f:
        result = json.load(f)
    
    true_cnt = true_cnt + len(result['induce_cids'])
    if not is_cond:
        continue

    if result['find_cid'] in result['induce_cids']:
        find_true_cnt = find_true_cnt + 1
    find_cnt = find_cnt +1 
print('begin to print results for sema-c')
print(find_cnt)
print(find_true_cnt)
print(true_cnt)


find_cnt = 0
find_true_cnt = 0
true_cnt = 0
for cid in cids:
    cid_path = os.path.join('./result/3',cid)
    result_path = os.path.join(cid_path,'path_result.json')
    result = {}
    with open(result_path) as f:
        result = json.load(f)
    is_cond = False
    for _,path_infos in result.items():
        for path_info in path_infos:
            if path_info['is_cond']:
                is_cond = True

    result_path = os.path.join(cid_path,'result.json')
    with open(result_path) as f:
        result = json.load(f)
    
    true_cnt = true_cnt + len(result['induce_cids'])
    if is_cond:
        continue

    if result['find_cid'] in result['induce_cids']:
        find_true_cnt = find_true_cnt + 1
    find_cnt = find_cnt +1 
print('begin to print results for sema-d')
print(find_cnt)
print(find_true_cnt)
print(true_cnt)



find_cnt = 0
find_true_cnt = 0
true_cnt = 0
for cid in cids:
    cid_path = os.path.join('./result/3',cid)
    result_path = os.path.join(cid_path,'path_result.json')
    result = {}
    with open(result_path) as f:
        result = json.load(f)
    is_cond = False
    cids_set = set()
    for _,path_infos in result.items():
        for path_info in path_infos:
            for line_cid in path_info['line_cids']:
                cids_set.add(line_cid)
    earliest_cid = getEarliestCid(cids_set,repo)
    result_path = os.path.join(cid_path,'result.json')
    with open(result_path) as f:
        result = json.load(f)
    true_cnt = true_cnt + len(result['induce_cids'])
    if earliest_cid in result['induce_cids']:
        find_true_cnt = find_true_cnt + 1

    find_cnt = find_cnt + 1
print('begin to print results for sema-l')
print(find_cnt)
print(find_true_cnt)
print(true_cnt)