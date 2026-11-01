import argparse
import bisect
import subprocess
import sys
from collections.abc import Callable, Sequence
from pathlib import Path

RUN_TIMEOUT_SECONDS: int = 120
KEY_MIN: int = -(2**31)
KEY_MAX: int = 2**31 - 1
IMPLS: tuple[str, ...] = ("vector", "set", "avl", "btree")


def generate(binary: Path, *arguments: str) -> list[list[str]]:
    completed = subprocess.run(
        [str(binary), *arguments],
        text=True,
        capture_output=True,
        timeout=RUN_TIMEOUT_SECONDS,
        check=True,
    )
    return [line.split() for line in completed.stdout.splitlines()]


def inserted_keys(commands: Sequence[list[str]]) -> list[int]:
    return [int(command[1]) for command in commands if command[0] == "k"]


def check_counts(binary: Path, solver: Path) -> None:
    commands = generate(binary, "--keys=1000", "--ops=500", "--insert-percent=10")
    assert len(commands) == 1500
    assert all(command[0] == "k" for command in commands[:1000])
    assert sum(command[0] == "k" for command in commands[1000:]) == 50
    assert sum(command[0] != "k" for command in commands[1000:]) == 450


def check_keys_are_distinct_and_fit_int(binary: Path, solver: Path) -> None:
    keys = inserted_keys(generate(binary, "--keys=5000", "--ops=100", "--insert-percent=50"))
    assert len(keys) == len(set(keys)) == 5050
    assert all(KEY_MIN <= key <= KEY_MAX for key in keys)


def check_seed(binary: Path, solver: Path) -> None:
    arguments = ("--keys=1000", "--ops=100")
    first = generate(binary, *arguments, "--seed=1")
    assert first == generate(binary, *arguments, "--seed=1")
    assert first != generate(binary, *arguments, "--seed=2")


def check_orders(binary: Path, solver: Path) -> None:
    ascending = inserted_keys(generate(binary, "--keys=1000", "--order=ascending"))
    assert ascending == sorted(ascending)
    descending = inserted_keys(generate(binary, "--keys=1000", "--order=descending"))
    assert descending == sorted(descending, reverse=True)
    shuffled = inserted_keys(generate(binary, "--keys=1000", "--order=random"))
    assert shuffled not in (sorted(shuffled), sorted(shuffled, reverse=True))


def check_clusters(binary: Path, solver: Path) -> None:
    clusters = 10
    keys = inserted_keys(
        generate(binary, "--keys=1000", "--order=clustered", f"--clusters={clusters}")
    )
    assert keys != sorted(keys)
    descents = sum(second < first for first, second in zip(keys, keys[1:]))
    assert 0 < descents <= clusters - 1
    position = {key: index for index, key in enumerate(sorted(keys))}
    for start in range(0, len(keys), len(keys) // clusters):
        run = [position[key] for key in keys[start : start + len(keys) // clusters]]
        assert run == list(range(run[0], run[0] + len(run)))


def check_query_kinds(binary: Path, solver: Path) -> None:
    for kind in "nmq":
        commands = generate(binary, "--keys=100", "--ops=100", f"--queries={kind}")
        assert {command[0] for command in commands[100:]} == {kind}


def check_indices_fit_current_size(binary: Path, solver: Path) -> None:
    commands = generate(binary, "--keys=10", "--ops=2000", "--insert-percent=50", "--queries=m")
    size = 0
    for command in commands:
        if command[0] == "k":
            size += 1
        else:
            assert 1 <= int(command[1]) <= size


def check_range_width(binary: Path, solver: Path) -> None:
    for percent, width in (("0.1", 1), ("1", 10), ("50", 500), ("100", 999)):
        commands = generate(
            binary, "--keys=1000", "--ops=300", "--queries=q", f"--range-percent={percent}"
        )
        keys = sorted(inserted_keys(commands))
        for command in commands[1000:]:
            low, high = int(command[1]), int(command[2])
            count = bisect.bisect_right(keys, high) - bisect.bisect_right(keys, low)
            assert count == width, f"percent {percent}, range {low} {high}: {count}"


def check_structures_agree(binary: Path, solver: Path) -> None:
    for order in ("random", "ascending", "descending", "clustered"):
        commands = generate(
            binary, "--keys=2000", "--ops=2000", "--insert-percent=30", f"--order={order}"
        )
        text = "\n".join(" ".join(command) for command in commands) + "\n"
        outputs = set()
        for impl in IMPLS:
            completed = subprocess.run(
                [str(solver), f"--impl={impl}"],
                input=text,
                text=True,
                capture_output=True,
                timeout=RUN_TIMEOUT_SECONDS,
                check=False,
            )
            assert completed.returncode == 0, f"{impl} {order}: {completed.stderr}"
            outputs.add(completed.stdout)
        assert len(outputs) == 1, f"{order}: structures disagree"


def check_bad_arguments(binary: Path, solver: Path) -> None:
    for arguments in (
        (),
        ("--keys=0",),
        ("--keys=abc",),
        ("--unknown=1",),
        ("--keys=10", "--insert-percent=101"),
        ("--keys=10", "--order=sideways"),
        ("--keys=10", "--queries=x"),
        ("--keys=10", "--range-percent=0"),
        ("--keys=10", "--clusters=0"),
    ):
        completed = subprocess.run(
            [str(binary), *arguments],
            text=True,
            capture_output=True,
            timeout=RUN_TIMEOUT_SECONDS,
            check=False,
        )
        assert completed.returncode == 1, f"{arguments}"
        assert completed.stdout == "", f"{arguments}"
        assert completed.stderr.startswith("Usage: "), f"{arguments}"


CHECKS: tuple[Callable[[Path, Path], None], ...] = (
    check_counts,
    check_keys_are_distinct_and_fit_int,
    check_seed,
    check_orders,
    check_clusters,
    check_query_kinds,
    check_indices_fit_current_size,
    check_range_width,
    check_structures_agree,
    check_bad_arguments,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--solver", type=Path, required=True)
    arguments = parser.parse_args()

    failed = 0
    for check in CHECKS:
        try:
            check(arguments.binary, arguments.solver)
        except AssertionError as error:
            failed += 1
            print(f"FAILED: {check.__name__}: {error}")
    print(f"generator: {len(CHECKS) - failed} of {len(CHECKS)} checks passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
