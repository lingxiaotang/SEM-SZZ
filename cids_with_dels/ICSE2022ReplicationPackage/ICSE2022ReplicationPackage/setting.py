import sys
import os

WORK_DIR = '/data1/xxxx'

REPOS_DIR = os.path.join(WORK_DIR, 'repos')

REPLICATION_DIR = os.path.join(WORK_DIR, 'ICSE2022ReplicationPackage')

DATA_FOLDER = os.path.join(REPLICATION_DIR, 'data')

SZZ_FOLDER = os.path.join(REPLICATION_DIR, 'icse2021-szz-replication-package')

DEFAULT_MAX_CHANGE_SIZE = sys.maxsize

AST_MAP_PATH = os.path.join(REPLICATION_DIR, 'ASTMapEval_jar')

LOG_DIR = os.path.join(REPLICATION_DIR, 'GitLogs')