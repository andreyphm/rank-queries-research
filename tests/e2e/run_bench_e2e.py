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


def check_resume_tells_build_from_read(script: Path, build_dir: Path) -> None:
    with tempfile.TemporaryDirectory() as directory:
        output_dir = Path(directory)
        arguments = ("--max-keys=100000", "--experiments", "btree", "--runs=1")
        run_script(script, build_dir, output_dir, *arguments)
        path = output_dir / "btree.csv"
        rows = read_rows(path)
        assert len(rows) == 5 * 3, len(rows)

        kept = [row for row in rows if not (row["ops"] != "0" and row["insert_percent"] == "0")]
        assert len(kept) == 5 * 2, len(kept)
        with open(path, "w", newline="", encoding="utf-8") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(kept)

        run_script(script, build_dir, output_dir, *arguments)
        resumed = read_rows(path)
        assert len(resumed) == 5 * 3, len(resumed)


def largest_planned_sizes(script: Path, build_dir: Path, experiment: str) -> dict[str, int]:
    with tempfile.TemporaryDirectory() as directory:
        output_dir = Path(directory)
        completed = subprocess.run(
            [
                sys.executable,
                str(script),
                f"--build-dir={build_dir}",
                f"--output-dir={output_dir}",
                "--experiments",
                experiment,
                "--dry-run",
            ],
            capture_output=True,
            text=True,
            timeout=RUN_TIMEOUT_SECONDS,
            check=True,
        )
        assert not list(output_dir.iterdir()), "dry run must not write results"
    largest: dict[str, int] = {}
    for line in completed.stdout.splitlines()[1:]:
        impl = line.split()[0]
        keys = line.split("keys=[")[1].rstrip("]").split(", ")
        largest[impl] = max(largest.get(impl, 0), *(int(key) for key in keys))
    return largest


def check_size_limits_in_plan(script: Path, build_dir: Path) -> None:
    expected = {
        "build": {"vector": 10**7, "set": 10**7, "avl": 10**7, "btree32": 10**7},
        "read_size": {"vector": 10**7, "set": 10**5, "avl": 10**7, "btree32": 10**7},
        "mix": {"vector": 10**6, "set": 10**5, "avl": 10**7, "btree32": 10**7},
        "btree": {"btree8": 10**7, "btree128": 10**7},
    }
    for experiment, sizes in expected.items():
        largest = largest_planned_sizes(script, build_dir, experiment)
        for impl, limit in sizes.items():
            assert largest.get(impl) == limit, f"{experiment} {impl}: {largest.get(impl)}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--script", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    arguments = parser.parse_args()

    checks = (
        check_rows_and_resume,
        check_timeout_stops_larger_sizes,
        check_resume_tells_build_from_read,
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
