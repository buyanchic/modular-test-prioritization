# Modular test prioritization system

## Authors and Contributors
The main contributors Yana Smirnova., student of SPbPU ICSC.
The advisor and contributor Vladimir A. Parkhomenko, Senior Lecturer of SPbPU ICSC.

## Introduction
This is a research project for hybrid unit test prioritization.

The program determines a unit test execution order that maximizes the speed of defect 
detection and evaluates the quality of the order using the APFD and APFD_c metrics.

## Link to source
https://digitalcommons.unl.edu/cgi/viewcontent.cgi?article=1017&context=csearticles

## License
MIT License
Input datasets used in this repository remain under the original licenses specified by their respective authors and sources:

## Warranty
The developed software is in progress. Authors give no warranty.

## Build and launch (Linux / macOS / Windows)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Only a C++17 compiler and CMake ≥ 3.16 are required. There are no external dependencies.
Python 3 is needed only for the `reference` acceptance test (standard
library; see `requirements.txt`).

## Usage examples

```bash
# classic "additional greedy" approach using a standard third-party example
./build/tcp --input data/rothermel2001.json --strategy additional

# evaluation of the given ordering (comparison with published APFD values)
./build/tcp --input data/rothermel2001.json --order C,E,B,A,D --json

# hybrid heuristic incorporating cost and history, with a step log
./build/tcp --input data/author_example.json --strategy hybrid --beta 0.5 --cost --trace

# importing the same example from CSV
./build/tcp --input data/author_example.csv --strategy hybrid

python3 scripts/check_reference.py --exe build/tcp \
            --input data/rothermel2001.json \
            --expected data/rothermel2001.expected.json
```

## Project structure

```
CMakeLists.txt              building and registering tests in CTest
requirements.txt            auxiliary script dependencies
src/tcp.hpp                 Core: model, CSV/JSON loading, algorithms, metrics
src/main.cpp                console interface
tests/tcp_tests.cpp         kernel unit tests (11 test groups)
scripts/check_reference.py  acceptance verification against the reference output
data/rothermel2001.json     reference input (external example, IEEE TSE 2001)
data/rothermel2001.expected.json  reference output
data/author_example.json    illustrative example by the author
data/author_example.csv     It's in CSV format
```

## Input data formats

The problem size is not fixed: the number of tests and covered entities is determined by the file's content.

**JSON**

```json
{
  "name": "suite",
  "entities": ["e1", "e2"],
  "tests":  [{"id": "T1", "cost": 1.0, "history": 0.1, "covers": ["e1"]}],
  "faults": [{"id": "F1", "detected_by": ["T1"], "severity": 1.0}]
}
```

The `faults` section is optional; without it, the APFD/APFD_c metrics are not calculated.

**CSV**

```
test,cost,history,e1,e2
T1,1.0,0.10,1,0
```

