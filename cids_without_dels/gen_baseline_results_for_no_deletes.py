import os
import sys
import json
import logging as log
from settings import *
sys.path.append('./ICSE2022ReplicationPackage/icse2021-szz-replication-package/tools/pyszz')
from szz.ag_szz import AGSZZ
from szz.b_szz import BaseSZZ
from szz.ma_szz import MASZZ, DetectLineMoved

dataset1_cid_info = []
with open('./dataset/dataset1_cid_info.json') as f:
    dataset1_cid_info = json.load(f)

dataset2_cid_info = []
with open('./dataset/dataset2_cid_info.json') as f:
    dataset2_cid_info = json.load(f)

abaseszz = BaseSZZ(repo_full_name='linux', repo_url=None, repos_dir=REPO_DIR, use_temp_dir=False)
aagszz = AGSZZ(repo_full_name='linux', repo_url=None, repos_dir=REPO_DIR, use_temp_dir=False)
amaszz = MASZZ(repo_full_name='linux', repo_url=None, repos_dir=REPO_DIR, use_temp_dir=False)

b_find_cnt = 0
b_true_cnt = 0
a_find_cnt = 0
a_true_cnt = 0
m_find_cnt = 0
m_true_cnt = 0
true_cnt = 0
cnt = 0
for cid_info in dataset1_cid_info:
    try:
        fix_cid = cid_info['fix_commit_hash']
        cnt = cnt + 1
        
        induce_cids = cid_info['bug_commit_hash']
        true_cnt = true_cnt + len(induce_cids)

        b_impact_files = abaseszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'], only_deleted_lines=False)
        b_induce_cids = [cid.hexsha for cid in abaseszz.find_bic(fix_commit_hash=fix_cid,impacted_files=b_impact_files)]

        a_impact_files = aagszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'], only_deleted_lines=False)
        a_induce_cids = [cid.hexsha for cid in aagszz.find_bic(fix_commit_hash=fix_cid,impacted_files=a_impact_files)]

        ma_impact_files = amaszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'], only_deleted_lines=False)
        ma_induce_cids = [cid.hexsha for cid in aagszz.find_bic(fix_commit_hash=fix_cid,impacted_files=ma_impact_files)]
    
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
    
        with open('baseline_results_for_dataset1.txt','w') as f:       
            f.write(f'ab_find_cnt:{b_find_cnt}\n')
            f.write(f'ab_true_cnt:{b_true_cnt}\n')
            f.write(f'aa_find_cnt:{a_find_cnt}\n')
            f.write(f'aa_true_cnt:{a_true_cnt}\n')
            f.write(f'am_find_cnt:{m_find_cnt}\n')
            f.write(f'am_true_cnt:{m_true_cnt}\n')
            f.write(f'true_cnt:{true_cnt}\n')
    except:
        pass


b_find_cnt = 0
b_true_cnt = 0
a_find_cnt = 0
a_true_cnt = 0
m_find_cnt = 0
m_true_cnt = 0
true_cnt = 0
cnt = 0
for cid_info in dataset2_cid_info:
    try:
        fix_cid = cid_info['fix_commit_hash']
        cnt = cnt + 1
        
        induce_cids = cid_info['bug_commit_hash']
        true_cnt = true_cnt + len(induce_cids)

        b_impact_files = abaseszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'], only_deleted_lines=False)
        b_induce_cids = [cid.hexsha for cid in abaseszz.find_bic(fix_commit_hash=fix_cid,impacted_files=b_impact_files)]

        a_impact_files = aagszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'], only_deleted_lines=False)
        a_induce_cids = [cid.hexsha for cid in aagszz.find_bic(fix_commit_hash=fix_cid,impacted_files=a_impact_files)]

        ma_impact_files = amaszz.get_impacted_files(fix_commit_hash=fix_cid, file_ext_to_parse=['c', 'java', 'cpp', 'h', 'hpp'], only_deleted_lines=False)
        ma_induce_cids = [cid.hexsha for cid in aagszz.find_bic(fix_commit_hash=fix_cid,impacted_files=ma_impact_files)]
    
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
    
        with open('baseline_results_for_dataset2.txt','w') as f:       
            f.write(f'ab_find_cnt:{b_find_cnt}\n')
            f.write(f'ab_true_cnt:{b_true_cnt}\n')
            f.write(f'aa_find_cnt:{a_find_cnt}\n')
            f.write(f'aa_true_cnt:{a_true_cnt}\n')
            f.write(f'am_find_cnt:{m_find_cnt}\n')
            f.write(f'am_true_cnt:{m_true_cnt}\n')
            f.write(f'true_cnt:{true_cnt}\n')
    except:
        pass