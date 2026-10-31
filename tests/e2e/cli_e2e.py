import argparse
import bisect
import random
import sys
from collections.abc import Callable, Sequence
from pathlib import Path

from common import Case, run_cases

KEY_MIN: int = -(2**31)
KEY_MAX: int = 2**31 - 1

MIXED_KEY_COUNT: int = 100_000
MIXED_QUERY_COUNT: int = 10_000
MIXED_KEY_BOUND: int = 100_000

BIG_KEY_COUNT: int = 2_000_000
BIG_QUERY_COUNT: int = 1_000_000


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


def random_query(
    generator: random.Random, keys: Sequence[int], low: int, high: int
) -> tuple[str, int]:
    kind = generator.choice("nqm")
    if kind == "m" and keys:
        index = generator.randint(1, len(keys))
        return f"m {index}", keys[index - 1]
    if kind == "q":
        first = generator.randint(low, high)
        second = generator.randint(low, high)
        count = (
            bisect.bisect_right(keys, second) - bisect.bisect_right(keys, first)
            if first < second
            else 0
        )
        return f"q {first} {second}", count
    key = generator.randint(low, high)
    return f"n {key}", bisect.bisect_left(keys, key)


def encode_case(name: str, commands: Sequence[str], answers: Sequence[int]) -> Case:
    return Case(
        name,
        "\n".join(commands) + "\n",
        0,
        " ".join(str(answer) for answer in answers) + "\n",
    )


def mixed_case(name: str, seed: int) -> Case:
    generator = random.Random(seed)
    is_insertion = [True] * MIXED_KEY_COUNT + [False] * MIXED_QUERY_COUNT
    generator.shuffle(is_insertion)

    keys: list[int] = []
    commands: list[str] = []
    answers: list[int] = []
    for insertion in is_insertion:
        if insertion:
            key = generator.randint(-MIXED_KEY_BOUND, MIXED_KEY_BOUND)
            position = bisect.bisect_left(keys, key)
            if position == len(keys) or keys[position] != key:
                keys.insert(position, key)
            commands.append(f"k {key}")
        else:
            command, answer = random_query(
                generator, keys, -MIXED_KEY_BOUND, MIXED_KEY_BOUND
            )
            commands.append(command)
            answers.append(answer)
    return encode_case(name, commands, answers)


def big_case(
    name: str, seed: int, inserted: Sequence[int], low: int, high: int
) -> Case:
    generator = random.Random(seed)
    keys = sorted(set(inserted))
    commands = [f"k {key}" for key in inserted]
    answers: list[int] = []
    for _ in range(BIG_QUERY_COUNT):
        command, answer = random_query(generator, keys, low, high)
        commands.append(command)
        answers.append(answer)
    return encode_case(name, commands, answers)


def scale_mixed_cases() -> Sequence[Case]:
    return (mixed_case("mixed insertions and queries", 1),)


def scale_big_cases() -> Sequence[Case]:
    generator = random.Random(2)
    uniform = [generator.randint(KEY_MIN, KEY_MAX) for _ in range(BIG_KEY_COUNT)]
    return (
        big_case("uniform keys", 3, uniform, KEY_MIN, KEY_MAX),
        big_case("ascending keys", 4, range(BIG_KEY_COUNT), -1, BIG_KEY_COUNT),
    )


GROUPS: dict[str, Callable[[], Sequence[Case]]] = {
    "basic": lambda: BASIC_CASES,
    "edge": lambda: EDGE_CASES,
    "input": lambda: INPUT_CASES,
    "scale_mixed": scale_mixed_cases,
    "scale_big": scale_big_cases,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--impl", required=True)
    parser.add_argument("--group", choices=GROUPS, required=True)
    arguments = parser.parse_args()
    return run_cases(
        arguments.binary, arguments.impl, arguments.group, GROUPS[arguments.group]()
    )


if __name__ == "__main__":
    sys.exit(main())
