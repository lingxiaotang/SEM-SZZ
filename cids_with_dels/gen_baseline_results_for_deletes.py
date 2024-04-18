# %%
import os
import sys
import json
import logging as log
sys.path.append('./ICSE2022ReplicationPackage/ICSE2022ReplicationPackage/icse2021-szz-replication-package/tools/pyszz')
from szz.ag_szz import AGSZZ
from szz.b_szz import BaseSZZ
from szz.ma_szz import MASZZ, DetectLineMoved
import random
from settings import *

baseszz = BaseSZZ(repo_full_name='linux', repo_url=None, repos_dir=REPO_DIR, use_temp_dir=False)
agszz = AGSZZ(repo_full_name='linux', repo_url=None, repos_dir=REPO_DIR, use_temp_dir=False)
maszz = MASZZ(repo_full_name='linux', repo_url=None, repos_dir=REPO_DIR, use_temp_dir=False)

b_find_cnt = 0
b_true_cnt = 0
a_find_cnt = 0
a_true_cnt = 0
m_find_cnt = 0
m_true_cnt = 0
true_cnt = 0
cnt = 0
test_commits = []
with open('./dataset/dataset3_cid_info.json') as f:
    test_commits = json.load(f)
for cid_info in test_commits:
    try:
        fix_cid = cid_info['fix_commit_hash']
        cnt = cnt + 1
        print(f'begin to deal with the {cnt} commit, total {len(test_commits)} commits')
        if os.path.exists(os.path.join('./baseline_results_for_deletes/',fix_cid)):
            continue
        
        induce_cids = cid_info['bug_commit_hash']
        true_cnt = true_cnt + len(induce_cids)

        b_impact_files = baseszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'],only_deleted_lines=True)
        b_induce_cids = [cid.hexsha for cid in baseszz.find_bic(fix_commit_hash=fix_cid,impacted_files=b_impact_files)]

        a_impact_files = agszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'],only_deleted_lines=True)
        a_induce_cids = [cid.hexsha for cid in agszz.find_bic(fix_commit_hash=fix_cid,impacted_files=a_impact_files)]

        ma_impact_files = maszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'],only_deleted_lines=True)
        ma_induce_cids = [cid.hexsha for cid in agszz.find_bic(fix_commit_hash=fix_cid,impacted_files=ma_impact_files)]
    
        b_find_cnt = b_find_cnt + len(b_induce_cids)
        a_find_cnt = a_find_cnt + len(a_induce_cids)
        m_find_cnt = m_find_cnt + len(ma_induce_cids)

        for cid in b_induce_cids:
            if cid in induce_cids:
                b_true_cnt = b_true_cnt + 1

        for cid in a_induce_cids:
            if cid in induce_cids:
                a_true_cnt = a_true_cnt + 1

        for cid in ma_induce_cids:
            if cid in induce_cids:
                m_true_cnt = m_true_cnt + 1
    
        with open('baseline_result.txt','w') as f:       
            f.write(f'b_find_cnt:{b_find_cnt}\n')
            f.write(f'b_true_cnt:{b_true_cnt}\n')
            f.write(f'a_find_cnt:{a_find_cnt}\n')
            f.write(f'a_true_cnt:{a_true_cnt}\n')
            f.write(f'm_find_cnt:{m_find_cnt}\n')
            f.write(f'm_true_cnt:{m_true_cnt}\n')
            f.write(f'true_cnt:{true_cnt}\n')

    except:
        pass