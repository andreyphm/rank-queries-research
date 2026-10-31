import argparse
import sys
from collections.abc import Sequence
from pathlib import Path

from common import Case, run_cases

KEY_MIN: int = -(2**31)
KEY_MAX: int = 2**31 - 1


def out_of_range(*indices: int) -> str:
    return "".join(f"Error: Index {index} is out of range\n" for index in indices)


BASIC_CASES: tuple[Case, ...] = (
    Case("slide example", "k 8 k 2 k -1 m 1 m 2 n 3 q -1 8\n", 0, "-1 2 2 2\n"),
    Case("empty input", "", 0, ""),
    Case("only insertions", "k 1 k 2 k 3\n", 0, ""),
    Case("queries on empty set", "n 5 q 1 2\n", 0, "0 0\n"),
    Case("one command per line", "k 1\nk 2\nn 2\nm 2\n", 0, "1 2\n"),
    Case("answers depend on order", "k 3 n 5 k 4 n 5\n", 0, "1 2\n"),
    Case("insertion order does not matter", "k 3 k 1 k 2 m 1 m 2 m 3\n", 0, "1 2 3\n"),
)

EDGE_CASES: tuple[Case, ...] = (
    Case("repeated insertion", "k 5 k 5 k 5 n 6\n", 0, "1\n"),
    Case("repeated insertion keeps order", "k 2 k 1 k 2 m 2\n", 0, "2\n"),
    Case("low equal to high", "k 1 k 2 q 1 1\n", 0, "0\n"),
    Case("low greater than high", "k 1 k 2 q 2 1\n", 0, "0\n"),
    Case("range excludes low and includes high", "k 1 k 2 k 3 q 1 3\n", 0, "2\n"),
    Case("count excludes the key itself", "k 1 k 2 n 2\n", 0, "1\n"),
    Case("negative keys", "k -3 k -1 k -2 m 1 n -1 q -3 -1\n", 0, "-3 2 2\n"),
    Case(
        "int bounds",
        f"k {KEY_MAX} k {KEY_MIN} m 1 m 2 n {KEY_MAX} q {KEY_MIN} {KEY_MAX}\n",
        0,
        f"{KEY_MIN} {KEY_MAX} 1 1\n",
    ),
    Case("zero index", "k 1 m 0\n", 1, "", out_of_range(0)),
    Case("index past the end", "k 1 m 2\n", 1, "", out_of_range(2)),
    Case("index on empty set", "m 1\n", 1, "", out_of_range(1)),
    Case("answers survive bad index", "k 1 m 2 m 1\n", 1, "1\n", out_of_range(2)),
    Case("several bad indices", "m 3 k 1 m 1 m 5\n", 1, "1\n", out_of_range(3, 5)),
)

INPUT_CASES: tuple[Case, ...] = (
    Case("whitespace only", " \n\t \n", 0, ""),
    Case("unknown letter", "x\n", 1, "", "Error: Unknown command 'x'\n"),
    Case("unknown word", "abc\n", 1, "", "Error: Unknown command 'abc'\n"),
    Case("key is not a number", "k abc\n", 1, "", "Error: Invalid query 'k abc'\n"),
    Case("key is fractional", "k 1.5\n", 1, "", "Error: Invalid query 'k 1.5'\n"),
    Case(
        "key above int",
        f"k {KEY_MAX + 1}\n",
        1,
        "",
        f"Error: Invalid query 'k {KEY_MAX + 1}'\n",
    ),
    Case(
        "key below int",
        f"n {KEY_MIN - 1}\n",
        1,
        "",
        f"Error: Invalid query 'n {KEY_MIN - 1}'\n",
    ),
    Case("negative index", "m -1\n", 1, "", "Error: Invalid query 'm -1'\n"),
    Case("bad range low", "q a 2\n", 1, "", "Error: Invalid query 'q a 2'\n"),
    Case("bad range high", "q 1 b\n", 1, "", "Error: Invalid query 'q 1 b'\n"),
    Case("missing key", "k\n", 1, "", "Error: Missing argument in 'k'\n"),
    Case("missing range high", "q 1\n", 1, "", "Error: Missing argument in 'q 1'\n"),
    Case(
        "processing continues",
        "k abc k 1 n 5\n",
        1,
        "1\n",
        "Error: Invalid query 'k abc'\n",
    ),
    Case(
        "parse errors come before index errors",
        "m 9 x\n",
        1,
        "",
        "Error: Unknown command 'x'\n" + out_of_range(9),
    ),
)

GROUPS: dict[str, Sequence[Case]] = {
    "basic": BASIC_CASES,
    "edge": EDGE_CASES,
    "input": INPUT_CASES,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--impl", required=True)
    parser.add_argument("--group", choices=GROUPS, required=True)
    arguments = parser.parse_args()
    return run_cases(
        arguments.binary, arguments.impl, arguments.group, GROUPS[arguments.group]
    )


if __name__ == "__main__":
    sys.exit(main())
