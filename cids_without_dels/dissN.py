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
from typing import List, Set
from tempfile import mkdtemp

import git
from pydriller import ModificationType, GitRepository as PyDrillerGitRepo
import traceback

for i in range(1,6):
    cids = os.listdir(f'./result/{i}')
    find_cnt = 0
    find_true_cnt = 0
    true_cnt = 0
    for cid in cids:
        cid_path = os.path.join(f'./result/{i}',cid)
        result_path = os.path.join(cid_path,'result.json')
        if not os.path.exists(result_path):
            continue
        result = {}
        with open(result_path) as f:
            result = json.load(f)
        true_cnt = true_cnt + len(result['induce_cids'])
        if result['find_cid'] in result['induce_cids']:
            find_true_cnt = find_true_cnt + 1
        find_cnt = find_cnt + 1

    print(f'begin to print result with parameter N {i}')
    print(find_cnt)
    print(find_true_cnt)
    print(true_cnt)