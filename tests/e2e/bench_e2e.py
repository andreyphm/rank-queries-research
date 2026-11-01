import argparse
import re
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path

RUN_TIMEOUT_SECONDS: int = 120
IMPLS: tuple[str, ...] = (
    "vector",
    "set",
    "avl",
    "btree8",
    "btree16",
    "btree32",
    "btree64",
    "btree128",
)
STREAM: str = "k 5 k 1 k 9 n 3 k 7 m 2 q 1 9\n"


def run_bench(
    binary: Path, arguments: Sequence[str], input_text: str
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(binary), *arguments],
        input=input_text,
        text=True,
        capture_output=True,
        timeout=RUN_TIMEOUT_SECONDS,
        check=False,
    )


def check_every_impl_prints_times(binary: Path) -> list[str]:
    errors: list[str] = []
    for impl in IMPLS:
        completed = run_bench(binary, [f"--impl={impl}", "--setup-count=3"], STREAM)
        if completed.returncode != 0 or not re.fullmatch(r"\d+,\d+\n", completed.stdout):
            errors.append(f"{impl}: code {completed.returncode}, {completed.stdout!r}")
    return errors


def check_setup_count_bounds(binary: Path) -> list[str]:
    errors: list[str] = []
    for count, expected_code in ((0, 0), (7, 0), (8, 1)):
        completed = run_bench(binary, ["--impl=avl", f"--setup-count={count}"], STREAM)
        if completed.returncode != expected_code:
            errors.append(f"setup count {count}: code {completed.returncode}")
    return errors


def check_bad_input_is_rejected(binary: Path) -> list[str]:
    completed = run_bench(binary, ["--impl=avl", "--setup-count=0"], "k abc\n")
    if completed.returncode != 1 or completed.stdout != "":
        return [f"bad input: code {completed.returncode}, {completed.stdout!r}"]
    return []


def solver_answers(solver: Path, commands: Sequence[str]) -> list[str]:
    completed = subprocess.run(
        [str(solver), "--impl=avl"],
        input="".join(f"{command}\n" for command in commands),
        text=True,
        capture_output=True,
        timeout=RUN_TIMEOUT_SECONDS,
        check=True,
    )
    return completed.stdout.split()


def answers_by_phase(stdout: str) -> tuple[list[str], list[str]]:
    _, after_setup_label = stdout.split("setup answers:\n")
    setup, ops = after_setup_label.split("ops answers:\n")
    return setup.split(), ops.split()


def check_answers_match_solver(binary: Path, solver: Path, generator: Path) -> list[str]:
    errors: list[str] = []
    for order in ("random", "clustered"):
        stream = subprocess.run(
            [str(generator), "--keys=2000", "--ops=2000", "--insert-percent=30", f"--order={order}"],
            text=True,
            capture_output=True,
            timeout=RUN_TIMEOUT_SECONDS,
            check=True,
        ).stdout
        commands = stream.splitlines()
        all_answers = solver_answers(solver, commands)
        for count in (0, 1000, 2000, 2700, len(commands)):
            setup_expected = solver_answers(solver, commands[:count])
            ops_expected = all_answers[len(setup_expected) :]
            for impl in IMPLS:
                completed = run_bench(
                    binary,
                    [f"--impl={impl}", f"--setup-count={count}", "--print-answers"],
                    stream,
                )
                if completed.returncode != 0:
                    errors.append(f"{impl} {order} {count}: code {completed.returncode}")
                elif answers_by_phase(completed.stdout) != (setup_expected, ops_expected):
                    errors.append(f"{impl} {order} {count}: answers differ from solver")
    return errors


def check_printed_index_error_sets_code(binary: Path) -> list[str]:
    completed = run_bench(
        binary, ["--impl=avl", "--setup-count=1", "--print-answers"], "k 1 m 5 m 1\n"
    )
    if (
        completed.returncode != 1
        or answers_by_phase(completed.stdout) != ([], ["1"])
        or completed.stderr != "Error: Index 5 is out of range\n"
    ):
        return [f"bad index: code {completed.returncode}, {completed.stderr!r}"]
    return []


def check_bad_arguments(binary: Path) -> list[str]:
    errors: list[str] = []
    for arguments in (
        [],
        ["--impl=avl"],
        ["--setup-count=1"],
        ["--impl=btree", "--setup-count=1"],
        ["--impl=avl", "--setup-count=x"],
        ["--impl=avl", "--setup-count=1", "--unknown"],
    ):
        completed = run_bench(binary, arguments, "")
        if completed.returncode != 1 or not completed.stderr.startswith("Usage: "):
            errors.append(f"{arguments}: code {completed.returncode}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--solver", type=Path, required=True)
    parser.add_argument("--generator", type=Path, required=True)
    arguments = parser.parse_args()

    errors = [
        *check_every_impl_prints_times(arguments.binary),
        *check_setup_count_bounds(arguments.binary),
        *check_bad_input_is_rejected(arguments.binary),
        *check_answers_match_solver(arguments.binary, arguments.solver, arguments.generator),
        *check_printed_index_error_sets_code(arguments.binary),
        *check_bad_arguments(arguments.binary),
    ]
    for error in errors:
        print(f"FAILED: {error}")
    print("benchmark: failed" if errors else "benchmark: passed")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
