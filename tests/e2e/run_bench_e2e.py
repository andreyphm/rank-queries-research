import argparse
import csv
import subprocess
import sys
import tempfile
from pathlib import Path

RUN_TIMEOUT_SECONDS: int = 120
STRUCTURE_COUNT: int = 4
ORDER_COUNT: int = 4
INSERT_PERCENT_COUNT: int = 4


def run_script(script: Path, build_dir: Path, output_dir: Path, *arguments: str) -> None:
    subprocess.run(
        [
            sys.executable,
            str(script),
            f"--build-dir={build_dir}",
            f"--output-dir={output_dir}",
            *arguments,
        ],
        capture_output=True,
        text=True,
        timeout=RUN_TIMEOUT_SECONDS,
        check=True,
    )


def read_rows(path: Path) -> list[dict[str, str]]:
    with open(path, newline="", encoding="utf-8") as file:
        return list(csv.DictReader(file))


def check_rows_and_resume(script: Path, build_dir: Path) -> None:
    with tempfile.TemporaryDirectory() as directory:
        output_dir = Path(directory)
        arguments = ("--max-keys=1000", "--experiments", "build", "mix")

        run_script(script, build_dir, output_dir, *arguments, "--runs=2")
        build_rows = read_rows(output_dir / "build.csv")
        mix_rows = read_rows(output_dir / "mix.csv")
        assert len(build_rows) == STRUCTURE_COUNT * ORDER_COUNT * 2
        assert len(mix_rows) == STRUCTURE_COUNT * INSERT_PERCENT_COUNT * 2
        for row in (*build_rows, *mix_rows):
            assert row["status"] == "ok", row
            assert int(row["setup_ns"]) > 0, row
            assert int(row["ops_ns"]) >= 0, row
        assert {row["ops"] for row in mix_rows} == {"1000"}

        run_script(script, build_dir, output_dir, *arguments, "--runs=3")
        resumed = read_rows(output_dir / "build.csv")
        assert len(resumed) == STRUCTURE_COUNT * ORDER_COUNT * 3
        keys = [(row["impl"], row["order"], row["seed"]) for row in resumed]
        assert len(keys) == len(set(keys))


def check_timeout_stops_larger_sizes(script: Path, build_dir: Path) -> None:
    with tempfile.TemporaryDirectory() as directory:
        output_dir = Path(directory)
        run_script(
            script,
            build_dir,
            output_dir,
            "--max-keys=10000",
            "--experiments",
            "build",
            "--runs=2",
            "--limit=0.000001",
        )
        rows = read_rows(output_dir / "build.csv")
        assert len(rows) == STRUCTURE_COUNT * ORDER_COUNT, len(rows)
        for row in rows:
            assert row["status"] == "timeout", row
            assert row["keys"] == "1000", row
            assert row["seed"] == "1", row
            assert row["setup_ns"] == "", row


def check_size_limits_in_plan(script: Path, build_dir: Path) -> None:
    with tempfile.TemporaryDirectory() as directory:
        output_dir = Path(directory)
        completed = subprocess.run(
            [
                sys.executable,
                str(script),
                f"--build-dir={build_dir}",
                f"--output-dir={output_dir}",
                "--dry-run",
            ],
            capture_output=True,
            text=True,
            timeout=RUN_TIMEOUT_SECONDS,
            check=True,
        )
        assert not list(output_dir.iterdir()), "dry run must not write results"
    lines = completed.stdout.splitlines()
    largest = {"set ": 100000, "vector ": 1000000, "avl ": 10000000}
    for prefix, limit in largest.items():
        plans = [line for line in lines if line.strip().startswith(prefix)]
        assert plans, prefix
        for line in plans:
            keys = line.split("keys=[")[1].rstrip("]").split(", ")
            assert max(int(key) for key in keys) <= limit, line
        assert any(str(limit) in line for line in plans), prefix


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--script", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    arguments = parser.parse_args()

    checks = (
        check_rows_and_resume,
        check_timeout_stops_larger_sizes,
        check_size_limits_in_plan,
    )
    failed = 0
    for check in checks:
        try:
            check(arguments.script, arguments.build_dir)
        except AssertionError as error:
            failed += 1
            print(f"FAILED: {check.__name__}: {error}")
    print(f"run_bench: {len(checks) - failed} of {len(checks)} checks passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
