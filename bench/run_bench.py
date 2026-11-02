import argparse
import csv
import subprocess
import sys
import tempfile
import time
from collections.abc import Callable, Sequence
from dataclasses import dataclass
from pathlib import Path

DEFAULT_RUNS: int = 10
DEFAULT_LIMIT_SECONDS: float = 300.0
DEFAULT_CPU: int = 2

SIZES: tuple[int, ...] = (10**3, 10**4, 10**5, 10**6, 10**7)
LARGE_SIZES: tuple[int, ...] = (10**5, 10**6)
BTREE_SIZES: tuple[int, ...] = (10**5, 10**6, 10**7)
OPS_LIMIT: int = 10_000
RANGE_PERCENT: float = 1.0

STRUCTURES: tuple[str, ...] = ("vector", "set", "avl", "btree32")
BTREES: tuple[str, ...] = ("btree8", "btree16", "btree32", "btree64", "btree128")
ORDERS: tuple[str, ...] = ("random", "ascending", "descending", "clustered")
READ_ORDERS: tuple[str, ...] = ("ascending", "clustered")
READ_QUERIES: tuple[str, ...] = ("n", "m", "q")
INSERT_PERCENTS: tuple[int, ...] = (1, 10, 50, 90)
MAX_KEYS_BY_IMPL: dict[str, int] = {"set": 10**5, "vector": 10**6}
BUILD_MAX_KEYS_BY_IMPL: dict[str, int] = {"vector": 10**6}

FIELDS: tuple[str, ...] = (
    "experiment",
    "impl",
    "keys",
    "ops",
    "insert_percent",
    "order",
    "queries",
    "range_percent",
    "seed",
    "setup_ns",
    "ops_ns",
    "status",
)


@dataclass(frozen=True)
class Stream:
    keys: int
    ops: int
    insert_percent: int
    order: str
    queries: str


@dataclass(frozen=True)
class Point:
    impl: str
    stream: Stream


def ops_for(keys: int) -> int:
    return min(keys, OPS_LIMIT)


def build_stream(keys: int, order: str) -> Stream:
    return Stream(keys, 0, 0, order, "mixed")


def read_stream(keys: int, order: str, queries: str) -> Stream:
    return Stream(keys, ops_for(keys), 0, order, queries)


def mix_stream(keys: int, insert_percent: int) -> Stream:
    return Stream(keys, ops_for(keys), insert_percent, "random", "mixed")


def for_each_impl(
    impls: Sequence[str],
    streams: Sequence[Stream],
    max_keys_by_impl: dict[str, int] = MAX_KEYS_BY_IMPL,
) -> list[Point]:
    return [
        Point(impl, stream)
        for stream in streams
        for impl in impls
        if stream.keys <= max_keys_by_impl.get(impl, SIZES[-1])
    ]


def build_points() -> list[Point]:
    streams = [build_stream(keys, order) for keys in SIZES for order in ORDERS]
    sorted_vector = [Point("vector", build_stream(SIZES[-1], "ascending"))]
    return for_each_impl(STRUCTURES, streams, BUILD_MAX_KEYS_BY_IMPL) + sorted_vector


def read_size_points() -> list[Point]:
    streams = [
        read_stream(keys, "random", queries)
        for keys in SIZES
        for queries in READ_QUERIES
    ]
    sorted_vector = [
        Point("vector", read_stream(SIZES[-1], "ascending", queries))
        for queries in READ_QUERIES
    ]
    return for_each_impl(STRUCTURES, streams) + sorted_vector


def read_order_points() -> list[Point]:
    streams = [
        read_stream(keys, order, queries)
        for keys in LARGE_SIZES
        for order in READ_ORDERS
        for queries in READ_QUERIES
    ]
    return for_each_impl(STRUCTURES, streams)


def mix_points() -> list[Point]:
    streams = [
        mix_stream(keys, insert_percent)
        for keys in SIZES
        for insert_percent in INSERT_PERCENTS
    ]
    return for_each_impl(STRUCTURES, streams)


def btree_points() -> list[Point]:
    streams = [
        stream
        for keys in BTREE_SIZES
        for stream in (
            build_stream(keys, "random"),
            read_stream(keys, "random", "mixed"),
            mix_stream(keys, 50),
        )
    ]
    return for_each_impl(BTREES, streams)


EXPERIMENTS: dict[str, Callable[[], list[Point]]] = {
    "build": build_points,
    "read_size": read_size_points,
    "read_order": read_order_points,
    "mix": mix_points,
    "btree": btree_points,
}


Scenario = tuple[str, bool, int, str, str]


def scenario_of(impl: str, stream: Stream) -> Scenario:
    return (impl, stream.ops > 0, stream.insert_percent, stream.order, stream.queries)


def row_key(impl: str, stream: Stream, seed: int) -> tuple[str, ...]:
    return (
        impl,
        str(stream.keys),
        str(stream.ops),
        str(stream.insert_percent),
        stream.order,
        stream.queries,
        str(seed),
    )


def load_progress(
    path: Path,
) -> tuple[set[tuple[str, ...]], dict[Scenario, int]]:
    done: set[tuple[str, ...]] = set()
    blocked_from: dict[Scenario, int] = {}
    if not path.exists():
        return done, blocked_from
    with open(path, newline="", encoding="utf-8") as file:
        for row in csv.DictReader(file):
            stream = Stream(
                int(row["keys"]),
                int(row["ops"]),
                int(row["insert_percent"]),
                row["order"],
                row["queries"],
            )
            done.add(row_key(row["impl"], stream, int(row["seed"])))
            if row["status"] == "timeout":
                scenario = scenario_of(row["impl"], stream)
                blocked_from[scenario] = min(
                    stream.keys, blocked_from.get(scenario, stream.keys)
                )
    return done, blocked_from


def generate_stream(generator: Path, stream: Stream, seed: int, path: Path) -> None:
    with open(path, "w", encoding="utf-8") as file:
        subprocess.run(
            [
                str(generator),
                f"--keys={stream.keys}",
                f"--ops={stream.ops}",
                f"--insert-percent={stream.insert_percent}",
                f"--order={stream.order}",
                f"--queries={stream.queries}",
                f"--range-percent={RANGE_PERCENT}",
                f"--seed={seed}",
            ],
            stdout=file,
            check=True,
        )


def measure(
    binary: Path, impl: str, stream: Stream, path: Path, cpu: int, limit: float
) -> tuple[str, str, str]:
    command = [
        "taskset",
        "-c",
        str(cpu),
        str(binary),
        f"--impl={impl}",
        f"--setup-count={stream.keys}",
    ]
    try:
        with open(path, encoding="utf-8") as file:
            completed = subprocess.run(
                command,
                stdin=file,
                capture_output=True,
                text=True,
                timeout=limit,
                check=True,
            )
    except subprocess.TimeoutExpired:
        return "", "", "timeout"
    setup_ns, ops_ns = completed.stdout.strip().split(",")
    return setup_ns, ops_ns, "ok"


def print_plan(name: str, points: Sequence[Point]) -> None:
    sizes: dict[Scenario, list[int]] = {}
    for point in points:
        scenario = scenario_of(point.impl, point.stream)
        sizes.setdefault(scenario, []).append(point.stream.keys)
    print(f"{name}: {len(points)} points")
    for (impl, has_ops, insert_percent, order, queries), keys in sizes.items():
        print(
            f"  {impl} ops={has_ops} insert={insert_percent} order={order} "
            f"queries={queries} keys={sorted(keys)}"
        )


def run_experiment(
    name: str, points: Sequence[Point], arguments: argparse.Namespace
) -> None:
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    path = arguments.output_dir / f"{name}.csv"
    done, blocked_from = load_progress(path)
    generator = arguments.build_dir / "rank_queries_gen"
    binary = arguments.build_dir / "rank_queries_bench"

    selected = [point for point in points if point.stream.keys <= arguments.max_keys]
    streams = sorted({point.stream for point in selected}, key=lambda s: s.keys)

    is_new = not path.exists()
    with open(path, "a", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=FIELDS)
        if is_new:
            writer.writeheader()

        started = time.monotonic()
        for seed in range(1, arguments.runs + 1):
            for stream in streams:
                pending = [
                    point.impl
                    for point in selected
                    if point.stream == stream
                    and row_key(point.impl, stream, seed) not in done
                    and stream.keys
                    < blocked_from.get(scenario_of(point.impl, stream), sys.maxsize)
                ]
                if not pending:
                    continue

                with tempfile.TemporaryDirectory() as directory:
                    stream_path = Path(directory) / "stream.txt"
                    generate_stream(generator, stream, seed, stream_path)
                    for impl in pending:
                        setup_ns, ops_ns, status = measure(
                            binary,
                            impl,
                            stream,
                            stream_path,
                            arguments.cpu,
                            arguments.limit,
                        )
                        writer.writerow(
                            {
                                "experiment": name,
                                "impl": impl,
                                "keys": stream.keys,
                                "ops": stream.ops,
                                "insert_percent": stream.insert_percent,
                                "order": stream.order,
                                "queries": stream.queries,
                                "range_percent": RANGE_PERCENT,
                                "seed": seed,
                                "setup_ns": setup_ns,
                                "ops_ns": ops_ns,
                                "status": status,
                            }
                        )
                        file.flush()
                        if status == "timeout":
                            scenario = scenario_of(impl, stream)
                            blocked_from[scenario] = stream.keys
                            print(f"{name}: timeout {impl} {stream}", flush=True)
            print(
                f"{name}: run {seed} of {arguments.runs} done, "
                f"{time.monotonic() - started:.0f} s",
                flush=True,
            )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument(
        "--experiments", nargs="+", choices=EXPERIMENTS, default=list(EXPERIMENTS)
    )
    parser.add_argument("--runs", type=int, default=DEFAULT_RUNS)
    parser.add_argument("--limit", type=float, default=DEFAULT_LIMIT_SECONDS)
    parser.add_argument("--max-keys", type=int, default=SIZES[-1])
    parser.add_argument("--cpu", type=int, default=DEFAULT_CPU)
    parser.add_argument("--dry-run", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    for name in arguments.experiments:
        if arguments.dry_run:
            print_plan(name, EXPERIMENTS[name]())
        else:
            run_experiment(name, EXPERIMENTS[name](), arguments)
    return 0


if __name__ == "__main__":
    sys.exit(main())
