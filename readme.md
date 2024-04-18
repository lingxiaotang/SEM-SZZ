# Replication package

This repository contains datasets and source code of our paper "Enhancing Bug-Inducing Commit Identification: A Fine-Grained Semantic Analysis Approach
", which is submitted to IEEE Transactions on Software Engineering. 

## Structure

The repository is divided into three directories. The **cids_with_dels** directory contains datasets, source code, and experimental results related to bug-fixing commits that involve deleted lines. In contrast, the **cids_without_dels** directory contains datasets, source code, and experimental results for bug-fixing commits where no lines were deleted.
The third directory contains the CPP code used for analyzing the two versions of the program.

### cids_with_dels

_dataset_: This directory contains information corresponding to `dataset3` in our paper.

_result_: This directory contains the experimental results of SEMA-SZZ on `dataset3`.

_gen_baseline_results_for_deletes.py_: This Python script generates baseline experimental results for `dataset3`.

_gen_results_for_deletes.py_: This Python script is specifically designed to generate experimental results using SEMA-SZZ on `dataset3`.

### cids_without_dels

_dataset_: This directory contains the dataset information corresponding to `dataset1` and  `dataset2` in our paper.

_result_: This directory contains the experimental results of SEMA-SZZ with different parameters N ranging from 1 to 5..

_gen_baseline_results_for_no_deletes.py_: This script generates experimental results for SEMA-SZZ on `dataset1` and `dataset2`.

_gen_results_for_no_deletes.py_: This script is used to generated experimental results for our proposed baselines on `dataset1` and `dataset2` respectively.

_aSZZ.py_: This file is used to generate experimental results for the A-SZZ algorithm on on `dataset1` and `dataset2`.

_ablation.py_: This script is used to conduct the ablation study in our paper.

_dissN.py_: This file is used to show the impact of the parameter N in our paper.

### clang-tools-extra

_aszz-bin_: This directory contains the code for implementing the a-szz algorithm.

_gen-info_ and _gen-line-result_: These two directories are used to extract information from files when locating bug-inducing commits.

_gen-result_: This directory contains the core code of our method. Here we will disscuss it in detail.

    1. The BlockInfoGenerator.cpp and BlockInfoGenerator.cpp two files are used to generate information for basic blocks, such as the first line number and the last line number and so on. 

    2. The BlockMapper.cpp and BlockMapper.h two files are used to map unmodified lines and basic blocks between two versions of the program. 
    
    3.The constraint.cpp and constraint.h are used to represent the constraint in a path. 
    
    4. The identifierTable.cpp and identifierTable.h are used to represent the data flow of each variable in a path.

    5. The PathGenerator.cpp and PathGenerator.h are used to do program slicing.

    6. The StateCollector.cpp and StateCollector.h are used to collect the state of a path.

    7. The PathComparator.cpp and PathComparator.h are used to do state comparison and generate buggy statements.


## Environment

1. Python Environment Setup:
Install all packages listed in requirements.txt for the Python environment.

2. Compiling Source Code in clang-tools-extra Directory:
Follow the instructions provided in https://clang.llvm.org/docs/LibASTMatchersTutorial.html to compile the source code located in the clang-tools-extra directory.

3. Setting Paths in settings.py:
After compiling the source code, set their paths in the settings.py files located in both cids_with_dels and cids_without_dels directories.

## Pre-calculated results

### RQ 1
To replicate our results for RQ 1:

```
cd ./cids_without_dels

python aSZZ.py

python gen_baseline_results_for_no_deletes.py

python gen_results_for_no_deletes.py
```

### RQ 2

To replicate our results for RQ 2:

```
cd ./cids_with_dels

python gen_baseline_results_for_deletes.py

python gen_results_for_deletes.py
```

### RQ 3

To replicate our results for RQ 2:

```
cd ./cids_without_dels

python ablation.py
```
### discussion
To replicate our results for discussion:

```
cd ./cids_without_dels

python dissN.py
```