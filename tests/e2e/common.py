import subprocess
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

RUN_TIMEOUT_SECONDS: int = 120


@dataclass
class Case:
    name: str
    input_text: str
    expected_code: int
    expected_stdout: str
    expected_stderr: str = ""


@dataclass
class Failure:
    case: Case
    code: int
    stdout: str
    stderr: str


def run_case(binary: Path, impl: str, case: Case) -> Failure | None:
    completed = subprocess.run(
        [str(binary), f"--impl={impl}"],
        input=case.input_text,
        text=True,
        capture_output=True,
        timeout=RUN_TIMEOUT_SECONDS,
        check=False,
    )
    if (
        completed.returncode == case.expected_code
        and completed.stdout == case.expected_stdout
        and completed.stderr == case.expected_stderr
    ):
        return None
    return Failure(case, completed.returncode, completed.stdout, completed.stderr)


def shorten(text: str, limit: int = 400) -> str:
    return (
        text if len(text) <= limit else text[:limit] + f"... ({len(text)} characters)"
    )


def run_cases(binary: Path, impl: str, group: str, cases: Sequence[Case]) -> int:
    failures = [
        failure
        for case in cases
        if (failure := run_case(binary, impl, case)) is not None
    ]
    for failure in failures:
        print(f"FAILED: {failure.case.name}")
        print(f"  expected code {failure.case.expected_code}, got {failure.code}")
        print(f"  expected stdout: {shorten(failure.case.expected_stdout)!r}")
        print(f"  actual stdout:   {shorten(failure.stdout)!r}")
        print(f"  expected stderr: {shorten(failure.case.expected_stderr)!r}")
        print(f"  actual stderr:   {shorten(failure.stderr)!r}")
        print(f"  input: {shorten(failure.case.input_text)!r}")

    print(
        f"{impl} {group}: {len(cases) - len(failures)} of {len(cases)} scenarios passed"
    )
    return 1 if failures else 0
