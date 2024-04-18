import os
import sys
import json
import subprocess
import re
import hashlib
from log_generation import GitLog

from setting import *

from data_loader import JAVA_CVE_FIX_COMMITS, C_CVE_FIX_COMMITS, read_cve_commits, REPOS_DIR, JAVA_PROJECTS, C_PROJECTS, ANNOTATED_CVES
from git_analysis.analyze_git_logs import retrieve_git_logs, retrieve_git_logs_dict, get_ancestors, get_parent_tags, get_son_tags, traverse_affected_versions

repos_dir = REPOS_DIR
log_dir = LOG_DIR

def get_tags(repo_dir):
    output = GitLog().git_tag(repo_dir)
    tags = output.split('\n')

    commit_tag_map = {}
    for tag in tags:
        if tag.strip() == "":
            continue

        try:
            output = GitLog().git_show(repo_dir, tag)
        except Exception as e:
            print(tag, e)
            continue

        commit = None
        timestamp = None
        for line in output.split('\n'):
            if line.startswith('commit:'):
                commit = line[8:].strip()
            
            if line.startswith('timestamp:'):
                timestamp = line[11:].strip()
                break
        
        if commit is not None:
            commit_tag_map[commit] = tag

    return commit_tag_map

def generate_logs(repo_dir, output):
    log_str = GitLog().git_log(repo_dir)
    with open(output, 'w') as fout:
        fout.write(log_str)


def get_duplicate_commits(commit_id, commit_patch_map, patch_commit_map):
    if commit_id not in commit_patch_map:
        return []
    
    duplicated_commits = set()
    for h in commit_patch_map[commit_id]:
        for c in patch_commit_map[h]:
            if c == commit_id:
                continue

            s1 = set(commit_patch_map[commit_id])
            s2 = set(commit_patch_map[c])
            if s1 == s2 or s1.issubset(s2):
                duplicated_commits.add(c)

    return list(duplicated_commits)

def generate_vulnerable_versions(project, fixing_commit, inducing_commit):
    git_logs = retrieve_git_logs(os.path.join(log_dir, project+"-meta.log"), project)
    git_log_dict = retrieve_git_logs_dict(git_logs, project)
    
    commit_tag_map = get_tags(os.path.join(repos_dir, project))
    for commit_id in git_log_dict:
        if commit_id in commit_tag_map:
            git_log_dict[commit_id].set_tag(commit_tag_map[commit_id])
        
        if len(git_log_dict[commit_id].parent) == 0:
            git_log_dict[commit_id].set_tag("Initial Commit")
    
    with open(os.path.join(REPLICATION_DIR, f'data_commit_patch_map/{project}-commit-patch.json')) as fin1, \
            open(os.path.join(REPLICATION_DIR, f'data_commit_patch_map/{project}-patch-commit.json')) as fin2:
        commit_patch_map = json.load(fin1)
        patch_commit_map = json.load(fin2)

    try:
        fc_sons_tag = get_son_tags(git_log_dict, fixing_commit)

        duplicated_commits = get_duplicate_commits(fixing_commit, commit_patch_map, patch_commit_map)
        if len(duplicated_commits) > 0:
            for c in duplicated_commits:
                fc_sons_tag |= get_son_tags(git_log_dict, c)

        ic_sons_tag = get_son_tags(git_log_dict, inducing_commit)

        duplicated_commits = get_duplicate_commits(inducing_commit, commit_patch_map, patch_commit_map)
        for c in duplicated_commits:
            ic_sons_tag |= get_son_tags(git_log_dict, c)

        ic_sons_tag = set([t.tag for t in ic_sons_tag])
        fc_sons_tag = set([t.tag for t in fc_sons_tag])

        return ic_sons_tag - fc_sons_tag

    except Exception as e:
        print(e)
        return None

def generate_version_annoated(projects=None):
    records = []
    for project in ANNOTATED_CVES:
        if projects is not None and project not in projects: 
            continue

        print(project)
        try:
            git_logs = retrieve_git_logs(os.path.join(log_dir, project+"-meta.log"), project)
            git_log_dict = retrieve_git_logs_dict(git_logs, project)

            commit_tag_map = get_tags(os.path.join(repos_dir, project))
            for commit_id in git_log_dict:
                if commit_id in commit_tag_map:
                    git_log_dict[commit_id].set_tag(commit_tag_map[commit_id])
                
                if len(git_log_dict[commit_id].parent) == 0:
                    git_log_dict[commit_id].set_tag("Initial Commit")
            
            with open(os.path.join(REPLICATION_DIR, f'data_commit_patch_map/{project}-commit-patch.json')) as fin1, \
                open(os.path.join(REPLICATION_DIR, f'data_commit_patch_map/{project}-patch-commit.json')) as fin2:
                commit_patch_map = json.load(fin1)
                patch_commit_map = json.load(fin2)
           
        except Exception as e:
            print(e)
            continue
   
        for cve in ANNOTATED_CVES[project]:
            print(project, cve)
            fixing_commits = ANNOTATED_CVES[project][cve]['fixing_commits']
            for fixing_commit in fixing_commits:
                record = {'project': project, 'cve': cve}
                
                try:
                    parents_tag = get_parent_tags(git_log_dict, fixing_commit)
                    sons_tag = get_son_tags(git_log_dict, fixing_commit)

                    duplicated_commits = get_duplicate_commits(fixing_commit, commit_patch_map, patch_commit_map)
                    if len(duplicated_commits) > 0:
                        print(duplicated_commits)
                        # print(duplicate_commit_map[fixing_commit])
                        record['duplicated_commits'] = duplicated_commits
                        for c in duplicated_commits:
                            parents_tag |= get_parent_tags(git_log_dict, c)
                            sons_tag |= get_son_tags(git_log_dict, c)

                    record['fixing_commit'] = fixing_commit
                    record['parents_tag'] = sorted([t.tag for t in parents_tag])
                    record['sons_tag'] = sorted([t.tag for t in sons_tag])
                except Exception as e:
                    print(e)
                    continue

                commit_set = set()
                for file in fixing_commits[fixing_commit]:
                    for line in fixing_commits[fixing_commit][file]:
                        previous_commits = fixing_commits[fixing_commit][file][line]['Previous Commits']
                        vic = fixing_commits[fixing_commit][file][line]['Vulnerability Introducing Commit']

                        commit_set |= set(previous_commits)

                record['inducing_commits'] = [] 
                for c in commit_set:
                    parents_tag = get_parent_tags(git_log_dict, c)
                    sons_tag = get_son_tags(git_log_dict, c)

                    duplicated_commits = get_duplicate_commits(c, commit_patch_map, patch_commit_map)
                    for c2 in duplicated_commits:
                            parents_tag |= get_parent_tags(git_log_dict, c)
                            sons_tag |= get_son_tags(git_log_dict, c)

                    parent_tags = sorted([t.tag for t in parents_tag])
                    sons_tag = sorted([t.tag for t in sons_tag])
                    record['inducing_commits'].append({'commit_id': c, 
                                                        'duplicated_commits': duplicated_commits,
                                                        'parents_tag': parent_tags,
                                                        'sons_tag': sons_tag,
                                                        'affected_versions': sorted(set(sons_tag)-set(record['sons_tag']))})
                
                records.append(record)
    
    with open(os.path.join(REPLICATION_DIR, 'results/label-tag.json'), 'w') as fout:
        json.dump(records, fout, indent=4)


if __name__ == '__main__':
    projects = ['activemq']

    # Step 1: Generate git log
    # for project in projects:
    #     try:
    #         log_out = os.path.join(LOG_DIR, project+"-meta.log")
    #         generate_logs(os.path.join(repos_dir, project), log_out)
    #     except Exception as e:
    #         print(project, e)
        

    # Step 2: generate hashes for hunks in commits, see identify_duplicated_patch.py
    # from identify_duplicated_patch import batch_duplicate_detection
    # batch_duplicate_detection(projects)


    # Step 3: generate vulnerable version tags for the annoated projects
    generate_version_annoated(projects)

    