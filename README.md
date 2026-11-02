<h1 align="center">Rank Queries Research</h1>

<p align="center">
  Comparing four data structures on rank queries, from a thousand to 10 million keys
</p>

<p align="center">
  <img alt="C++20" src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white">
  <img alt="CMake" src="https://img.shields.io/badge/CMake-3.16%2B-064F8C?logo=cmake&logoColor=white">
  <img alt="Python" src="https://img.shields.io/badge/Python-3.10%2B-3776AB?logo=python&logoColor=white">
</p>

---

The program reads a stream of commands: insert a key, count keys less than a given one, count keys in a range, and find the i-th smallest key. It answers the queries in order, using one of four data structures chosen with the `--impl` flag. A benchmark compares the structures on the same command streams, from 1,000 to 10,000,000 keys.

- Four structures: a sorted `std::vector`, `std::set`, an AVL tree, and a B-tree.
- The solver and the benchmark run the same code, and the benchmark measures only the operations on the structure, without input and output.
- Keys are `int` values.
- Invalid commands are reported to stderr; the valid ones are still answered.

## Contents

- [Getting started](#getting-started)
- [Usage](#usage)
  - [Solver](#solver)
  - [Input format](#input-format)
  - [Output format](#output-format)
  - [Stream generator](#stream-generator)
  - [Benchmark](#benchmark)
- [Data structures](#data-structures)
- [Performance research](#performance-research)
- [Testing](#testing)
- [Project layout](#project-layout)

## Getting started

> [!NOTE]
> The build and tests are verified on Windows ([MSYS2](https://www.msys2.org/) UCRT64) and Linux (Ubuntu 24.04 under WSL).

Requirements:

| Tool | Version | Used for |
|---|---|---|
| GCC | C++20 support, tested with GCC 13 and 16 | building |
| [CMake](https://cmake.org/) | 3.16+ | building, fetching GoogleTest |
| [Ninja](https://ninja-build.org/) | any | build generator on MSYS2, where CMake uses it by default |
| Python | 3.10+ | e2e tests, running the benchmark |
| [matplotlib](https://matplotlib.org/) | any | plotting benchmark results |

GoogleTest is fetched automatically on the first configure.

```bash
git clone https://github.com/andreyphm/rank-queries-research.git
cd rank-queries-research
```

### Windows (MSYS2 UCRT64)

Install the toolchain from an MSYS2 UCRT64 terminal:

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python mingw-w64-ucrt-x86_64-python-matplotlib
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux (Ubuntu 24.04)

Install the toolchain:

```bash
sudo apt install -y build-essential cmake git python3 python3-matplotlib
```

Build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Usage

### Solver

The solver reads commands from stdin and prints answers to stdout. The `--impl` flag selects the structure: `vector`, `set`, `avl`, or `btree`. The default is `avl`.

Linux:

```bash
echo "k 8 k 2 k -1 m 1 m 2 n 3 q -1 8" | ./build/rank_queries --impl=btree
```

Windows:

```bash
cmd /c "echo k 8 k 2 k -1 m 1 m 2 n 3 q -1 8 | build\rank_queries.exe --impl=btree"
```

> [!TIP]
> PowerShell 5.1 adds a byte order mark to text sent through |, so the first command becomes invalid. That is why the programs are launched through `cmd /c`. In an MSYS2 terminal the Linux syntax works.

### Input format

Commands are separated by any whitespace:

| Command | Meaning | Answer |
|---|---|---|
| `k x` | insert key `x`; a repeated key is ignored | none |
| `n x` | count keys less than `x` | a number |
| `q l r` | count keys in the range $(l, r]$; for `l >= r` the answer is `0` | a number |
| `m i` | find the `i`-th smallest key, counting from `1` | a key |

### Output format

All answers on one line, separated by spaces. For the example above:

```text
-1 2 2 2
```

The solver writes an error to stderr and goes on in two cases: a command it cannot parse, and `m i` with `i` outside `[1, size]`. Answers to the valid queries are still printed, and the exit code becomes `1`:

```text
$ echo "k 1 m 2 m 1" | ./build/rank_queries
Error: Index 2 is out of range
1
```

### Stream generator

`rank_queries_gen` writes a random command stream to stdout: first `N` insertions, then `M` operations. The same seed always gives the same stream.

```bash
./build/rank_queries_gen --keys=1000000 --ops=10000 --order=clustered > stream.txt
./build/rank_queries --impl=btree < stream.txt
```

| Argument | Default | Meaning |
|---|---|---|
| `--keys=N` | required | number of distinct keys inserted first |
| `--ops=M` | `0` | number of operations after them |
| `--insert-percent=P` | `0` | share of insertions among the operations |
| `--order=` | `random` | order of insertions: `random`, `ascending`, `descending`, `clustered` |
| `--clusters=C` | `100` | number of groups for `clustered`: groups come in random order, keys inside a group come in ascending order |
| `--queries=` | `mixed` | queries among the operations: `n`, `m`, `q`, or `mixed` in equal shares |
| `--range-percent=W` | `1` | width of `q` ranges, percent of all keys |
| `--seed=S` | `1` | random seed |

### Benchmark

`rank_queries_bench` measures one run: it builds the structure from the first `K` commands of the stream, then runs the rest. It prints two times in nanoseconds, `setup_ns,ops_ns`.

```bash
./build/rank_queries_bench --impl=btree32 --setup-count=1000000 < stream.txt
```

The structures are `vector`, `set`, `avl`, and `btree8` to `btree128`, which are B-trees with the minimum degree `t` from 8 to 128.

[`bench/run_bench.py`](bench/run_bench.py) runs all experiments: it generates the streams, runs every structure on them, and appends each run to a CSV file. An interrupted run resumes from where it stopped.

```bash
python3 bench/run_bench.py --build-dir build --output-dir bench/results
```

| Argument | Default | Meaning |
|---|---|---|
| `--build-dir` | required | directory with the built programs |
| `--output-dir` | required | directory for CSV files |
| `--experiments` | all | `build`, `read_size`, `read_order`, `mix`, `btree` |
| `--runs` | `10` | runs of each point |
| `--limit` | `300` | time limit of one run, seconds |
| `--max-keys` | `10000000` | skip points with more keys |
| `--cpu` | `2` | core to pin the runs to |
| `--dry-run` | off | print the plan without running it |

[`bench/plot.py`](bench/plot.py) draws the plots and the throughput tables from the CSV files:

```bash
python3 bench/plot.py --results-dir bench/results --plots-dir bench/plots
```

> [!NOTE]
> The results in [bench/results/](bench/results) were measured on a Release build under WSL. On Windows the runs are pinned to a core too, but the numbers may differ because the compiler and the memory allocator are different.

## Data structures

Every structure supports the same four operations:

| Structure | `k` insert | `n` count less | `q` count in range | `m` i-th smallest |
|---|---|---|---|---|
| `vector` | $O(n)$ | $O(\log n)$ | $O(\log n)$ | $O(1)$ |
| `set` | $O(\log n)$ | $O(n)$ | $O(\log n + k)$ | $O(n)$ |
| `avl` | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ |
| `btree` | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ | $O(\log n)$ |

Here $k$ is the number of keys in the range.

### Sorted vector

Implementation: [include/structures/sorted_vector.hpp](include/structures/sorted_vector.hpp).

Keys are kept sorted in one array. Queries are binary searches, and the i-th smallest key is just the element at index `i - 1`. An insertion shifts every larger key one place to the right.

### Standard set

Implementation: [include/structures/set_wrapper.hpp](include/structures/set_wrapper.hpp).

`std::set` finds a key in $O(\log n)$, but it does not know the position of the key. To count keys or find the i-th one, it walks the elements one by one. For `q`, the walk covers only the range.

### AVL tree

Implementation: [include/structures/avl_tree.hpp](include/structures/avl_tree.hpp).

A balanced binary search tree: the heights of the two subtrees of any node differ by at most one. Each node also stores the size of its subtree.

An insertion goes down from the root and adds the new key as a leaf. On the way back up, every node on the path updates its height and size, and a rotation restores the balance if the insertion broke it. A repeated key stops the descent, and the tree does not change.

Counting the keys less than `x` is one descent from the root: every time the search goes right, the left subtree and the node itself are added to the count. A range query $(l, r]$ is two such descents: the number of keys up to `r` minus the number of keys up to `l`. The i-th smallest key is found by the same descent, comparing `i` with the size of the left subtree.

### B-tree

Implementation: [include/structures/btree.hpp](include/structures/btree.hpp).

Each node holds from $t - 1$ to $2t - 1$ sorted keys in one array, and a node with $m$ keys has $m + 1$ children. The parent also stores the subtree size of each child, next to the pointer to it.

An insertion first checks whether the key is already in the tree, because a repeated key must not change the sizes. Then it goes down from the root, adds one to the size of each subtree it goes into, and puts the key into a leaf. Full nodes are split on the way down, before the insertion goes into them. The tree gets a new level only when the root is split, so all leaves stay at the same depth.

Counting the keys less than `x` is one descent from the root. In each node it finds the position of `x` among the keys, adds the number of keys before that position and the sizes of the subtrees before it, and goes down into the subtree at that position. Since the sizes are stored in the parent, the descent does not visit the subtrees it skips. A range query is two such descents, as in the AVL tree. The i-th smallest key is found by the same descent, subtracting from `i` the sizes of the subtrees and the keys it passes.

The asymptotics match the AVL tree, but a node covers many keys in a few adjacent cache lines, and the tree is several times lower.

## Performance research

The research with plots and conclusions is in [bench/README.md](bench/README.md).

## Testing

All tests run through [CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) after building:

```bash
ctest --test-dir build --output-on-failure
```

### Unit tests

Written with [GoogleTest](https://github.com/google/googletest). The structure tests are typed: the same cases run for every structure. The B-tree is tested with `t = 2`, so that small inputs already cause splits. The parser, the runner, and the output have their own tests.

```bash
ctest --test-dir build -L unit
```

### End-to-end tests

Implementation: [tests/e2e/](tests/e2e).

Python scripts test the built programs as a whole. A script builds the input itself, runs the program, and checks the exit code, stdout, and stderr. The expected answers for large inputs come from a simple reference solution on a sorted list.

Each structure and group is a separate CTest test `e2e_<impl>_<group>`, labeled `e2e`:

| Group | What it checks | Structures |
|---|---|---|
| `basic` | the example from the task, simple cases | all |
| `edge` | repeated keys, `l >= r`, negative keys, `int` bounds, invalid indices | all |
| `input` | invalid commands and error messages | all |
| `scale_mixed` | 100,000 keys and 10,000 queries mixed together | all |
| `scale_big` | 2,000,000 keys, then 1,000,000 queries | `avl`, `btree` |

Three more tests check the tools:

| Test | What it checks |
|---|---|
| `e2e_gen` | the stream generator: command counts, distinct keys, orders, `q` widths, repeatability |
| `e2e_bench` | the benchmark program gives the same answers as the solver |
| `e2e_run_bench` | the experiment script: CSV rows and resuming |

All e2e tests:

```bash
ctest --test-dir build -L e2e
```

Tests of one structure:

```bash
ctest --test-dir build -R e2e_btree --output-on-failure
```

## Project layout

```text
.
├── CMakeLists.txt                      # root build
├── include/
│   ├── query.hpp                       # key type and query kinds
│   ├── answer.hpp                      # answer kinds
│   ├── parser.hpp                      # parsing commands
│   ├── runner.hpp                      # running queries on a structure
│   ├── output.hpp                      # printing answers
│   └── structures/
│       ├── sorted_vector.hpp           # sorted std::vector
│       ├── set_wrapper.hpp             # std::set
│       ├── avl_tree.hpp                # AVL tree with subtree sizes
│       └── btree.hpp                   # B-tree with subtree sizes
├── source/
│   ├── parser.cpp
│   ├── output.cpp
│   └── rank_queries.cpp                # solver
├── tools/
│   └── rank_queries_gen.cpp            # stream generator
├── bench/
│   ├── README.md                       # performance research
│   ├── rank_queries_bench.cpp          # timing one run
│   ├── run_bench.py                    # running the experiments
│   ├── plot.py                         # plots and tables from CSV
│   ├── results/                        # CSV files, one per experiment
│   └── plots/                          # plots and throughput tables
└── tests/
    ├── unit/
    │   ├── CMakeLists.txt
    │   ├── structures.hpp              # list of tested structures
    │   ├── structure_tests.cpp
    │   ├── parser_tests.cpp
    │   ├── runner_tests.cpp
    │   └── output_tests.cpp
    └── e2e/
        ├── CMakeLists.txt              # registers e2e tests with CTest
        ├── common.py                   # running a case and printing mismatches
        ├── cli_e2e.py                  # solver scenarios and the reference solution
        ├── gen_e2e.py                  # checks for the stream generator
        ├── bench_e2e.py                # checks for the benchmark program
        └── run_bench_e2e.py            # checks for the experiment script
```
