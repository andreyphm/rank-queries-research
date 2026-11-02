import argparse
import csv
import statistics
import sys
from collections.abc import Sequence
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker
from matplotlib.axes import Axes
from matplotlib.figure import Figure

SURFACE: str = "#fcfcfb"
INK: str = "#0b0b0b"
SECONDARY_INK: str = "#52514e"
MUTED_INK: str = "#898781"
GRID: str = "#e1e0d9"

STRUCTURES: tuple[str, ...] = ("vector", "set", "avl", "btree32")
STRUCTURE_COLORS: dict[str, str] = {
    "vector": "#2a78d6",
    "set": "#eb6834",
    "avl": "#1baf7a",
    "btree32": "#eda100",
}
SIZE_COLORS: dict[int, str] = {10**5: "#86b6ef", 10**6: "#2a78d6", 10**7: "#104281"}

SIZES: tuple[int, ...] = (10**3, 10**4, 10**5, 10**6, 10**7)
ORDERS: tuple[str, ...] = ("random", "ascending", "descending", "clustered")
READ_ORDERS: tuple[str, ...] = ("random", "ascending", "clustered")
QUERIES: tuple[str, ...] = ("n", "m", "q")
INSERT_PERCENTS: tuple[int, ...] = (1, 10, 50, 90)
BTREE_TS: tuple[int, ...] = (8, 16, 32, 64, 128)
OPS_LIMIT: int = 10_000

ORDER_TITLES: dict[str, str] = {
    "random": "random order",
    "ascending": "ascending",
    "descending": "descending",
    "clustered": "clustered",
}
QUERY_TITLES: dict[str, str] = {
    "n": "n x: keys less than x",
    "m": "m i: i-th smallest",
    "q": "q l r: keys in (l, r], width 1 %",
}

Key = tuple[str, int, int, int, str, str]


def load(path: Path) -> dict[Key, list[float]]:
    times: dict[Key, list[float]] = {}
    with open(path, newline="", encoding="utf-8") as file:
        for row in csv.DictReader(file):
            if row["status"] != "ok":
                continue
            keys, ops = int(row["keys"]), int(row["ops"])
            key = (
                row["impl"],
                keys,
                ops,
                int(row["insert_percent"]),
                row["order"],
                row["queries"],
            )
            per_unit = int(row["setup_ns"]) / keys if ops == 0 else int(row["ops_ns"]) / ops
            times.setdefault(key, []).append(per_unit)
    return times


def load_reads(path: Path) -> dict[Key, list[float]]:
    times = load(path)
    for (impl, keys, ops, insert_percent, order, queries), values in list(times.items()):
        if impl == "vector" and order == "ascending":
            times.setdefault((impl, keys, ops, insert_percent, "random", queries), values)
    return times


def median_of(times: dict[Key, list[float]], key: Key) -> float | None:
    values = times.get(key)
    if not values:
        return None
    return statistics.median(values)


def ops_for(keys: int) -> int:
    return min(keys, OPS_LIMIT)


def power_of_ten(keys: int) -> str:
    return f"$10^{{{len(str(keys)) - 1}}}$"


def style_axes(axes: Axes, title: str) -> None:
    axes.set_facecolor(SURFACE)
    axes.set_title(title, color=INK, fontsize=10)
    axes.grid(True, which="major", color=GRID, linewidth=0.6)
    axes.set_axisbelow(True)
    axes.tick_params(
        which="both", length=0, colors=MUTED_INK, labelcolor=SECONDARY_INK, labelsize=8
    )
    for spine in axes.spines.values():
        spine.set_color(GRID)


def drop_minor_ticks(axes: Axes) -> None:
    axes.xaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    axes.yaxis.set_minor_locator(matplotlib.ticker.NullLocator())
    axes.tick_params(which="both", length=0)


def new_figure(
    rows: int, columns: int, width: float, height: float, sharey: bool = True
) -> tuple[Figure, list[Axes]]:
    figure, axes = plt.subplots(
        rows, columns, figsize=(width, height), sharey=sharey, squeeze=False
    )
    figure.patch.set_facecolor(SURFACE)
    return figure, list(axes.flat)


def plot_series(
    axes: Axes, xs: Sequence[float], medians_by_x: Sequence[float | None], label: str, color: str
) -> None:
    points = [(x, median) for x, median in zip(xs, medians_by_x) if median is not None]
    if not points:
        return
    axes.plot(
        [x for x, _ in points],
        [median for _, median in points],
        color=color,
        linewidth=2,
        marker="o",
        markersize=6,
        markeredgecolor=SURFACE,
        markeredgewidth=1,
        label=label,
    )


def finish(figure: Figure, axes: Sequence[Axes], title: str, ylabel: str, path: Path) -> None:
    height = figure.get_size_inches()[1]
    handles, labels = axes[0].get_legend_handles_labels()
    figure.legend(
        handles,
        labels,
        loc="upper center",
        ncol=len(labels),
        frameon=False,
        bbox_to_anchor=(0.5, 1 - 0.38 / height),
        labelcolor=SECONDARY_INK,
    )
    figure.suptitle(title, color=INK, fontsize=12, y=1 - 0.08 / height)
    axes[0].set_ylabel(ylabel, color=SECONDARY_INK)
    for panel_axes in axes:
        drop_minor_ticks(panel_axes)
    figure.tight_layout(rect=(0, 0, 1, 1 - 0.62 / height))
    figure.savefig(path, dpi=150, facecolor=SURFACE)
    plt.close(figure)


def plot_by_size(
    times: dict[Key, list[float]],
    panels: Sequence[tuple[str, int, int, str, str]],
    columns: int,
    title: str,
    ylabel: str,
    path: Path,
) -> None:
    rows = (len(panels) + columns - 1) // columns
    figure, axes = new_figure(rows, columns, 4.2 * columns, 3.6 * rows + 0.8)
    for panel_axes, (panel_title, measured, insert_percent, order, queries) in zip(axes, panels):
        style_axes(panel_axes, panel_title)
        panel_axes.set_xscale("log")
        panel_axes.set_yscale("log")
        panel_axes.set_xlabel("N, keys", color=SECONDARY_INK, fontsize=9)
        for impl in STRUCTURES:
            medians_by_x = [
                median_of(
                    times,
                    (impl, keys, ops_for(keys) if measured else 0, insert_percent, order, queries),
                )
                for keys in SIZES
            ]
            plot_series(panel_axes, SIZES, medians_by_x, impl, STRUCTURE_COLORS[impl])
    for unused in axes[len(panels) :]:
        unused.set_visible(False)
    finish(figure, axes, title, ylabel, path)


def plot_build(results: Path, plots: Path) -> None:
    times = load(results / "build.csv")
    panels = [(ORDER_TITLES[order], 0, 0, order, "mixed") for order in ORDERS]
    plot_by_size(
        times, panels, 4, "Building a set of N keys", "ns per insertion", plots / "build.png"
    )


def plot_read_size(results: Path, plots: Path) -> None:
    times = load_reads(results / "read_size.csv")
    panels = [(QUERY_TITLES[queries], 1, 0, "random", queries) for queries in QUERIES]
    plot_by_size(
        times,
        panels,
        3,
        "Reads: min(N, $10^4$) queries on a built set",
        "ns per query",
        plots / "read_size.png",
    )


def plot_mix(results: Path, plots: Path) -> None:
    times = load(results / "mix.csv")
    panels = [
        (f"{percent} % insertions", 1, percent, "random", "mixed") for percent in INSERT_PERCENTS
    ]
    plot_by_size(
        times,
        panels,
        4,
        "Mixed insertions and queries (n, m, q in equal shares)",
        "ns per operation",
        plots / "mix.png",
    )


def plot_read_order(results: Path, plots: Path) -> None:
    times = load(results / "read_size.csv") | load(results / "read_order.csv")
    sizes = (10**5, 10**6)
    figure, axes = new_figure(len(sizes), len(QUERIES), 12.6, 7.6, sharey=False)
    for row, keys in enumerate(sizes):
        for column, queries in enumerate(QUERIES):
            panel_axes = axes[row * len(QUERIES) + column]
            style_axes(panel_axes, f"{QUERY_TITLES[queries]}, N = {power_of_ten(keys)}")
            panel_axes.set_yscale("log")
            medians = [
                median
                for impl in STRUCTURES
                for order in READ_ORDERS
                if (median := median_of(times, (impl, keys, ops_for(keys), 0, order, queries)))
            ]
            measured = [
                impl
                for impl in STRUCTURES
                if median_of(times, (impl, keys, ops_for(keys), 0, "random", queries))
            ]
            width = 0.8 / len(measured)
            for index, impl in enumerate(measured):
                positions, heights = [], []
                for group, order in enumerate(READ_ORDERS):
                    median = median_of(times, (impl, keys, ops_for(keys), 0, order, queries))
                    if median is not None:
                        positions.append(group - 0.4 + width * (index + 0.5))
                        heights.append(median)
                panel_axes.bar(
                    positions,
                    heights,
                    width=width,
                    color=STRUCTURE_COLORS[impl],
                    edgecolor=SURFACE,
                    linewidth=1,
                    label=impl,
                )
            panel_axes.set_ylim(min(medians) / 2, max(medians) * 2)
            panel_axes.set_xlim(-0.5, len(READ_ORDERS) - 0.5)
            panel_axes.set_xticks(range(len(READ_ORDERS)))
            panel_axes.set_xticklabels([ORDER_TITLES[order] for order in READ_ORDERS], fontsize=8)
    finish(
        figure,
        axes,
        "Reads after building in different key orders",
        "ns per query",
        plots / "read_order.png",
    )


def plot_btree(results: Path, plots: Path) -> None:
    times = load(results / "btree.csv")
    panels = (
        ("Build, ns per insertion", 0, 0),
        ("Reads n, m, q, ns per query", 1, 0),
        ("50 % insertions, ns per operation", 1, 50),
    )
    figure, axes = new_figure(1, len(panels), 12.6, 4.2)
    for panel_axes, (panel_title, measured, insert_percent) in zip(axes, panels):
        style_axes(panel_axes, panel_title)
        panel_axes.set_xscale("log", base=2)
        panel_axes.set_xticks(BTREE_TS)
        panel_axes.set_xticklabels([str(t) for t in BTREE_TS])
        panel_axes.set_xlabel("t, minimum degree", color=SECONDARY_INK, fontsize=9)
        panel_axes.tick_params(axis="y", labelleft=True)
        for keys, color in SIZE_COLORS.items():
            medians_by_x = [
                median_of(
                    times,
                    (f"btree{t}", keys, ops_for(keys) if measured else 0, insert_percent, "random", "mixed"),
                )
                for t in BTREE_TS
            ]
            plot_series(panel_axes, BTREE_TS, medians_by_x, f"N = {power_of_ten(keys)}", color)
    for panel_axes in axes:
        panel_axes.set_ylim(bottom=0)
    finish(figure, axes, "B-tree: choosing t", "ns", plots / "btree.png")


def format_throughput(median: float | None) -> str:
    if median is None:
        return "—"
    return f"{1e3 / median:.3g}"


def throughput_table(
    times: dict[Key, list[float]], insert_percent: int, order: str, queries: str
) -> list[str]:
    lines = [
        "| N | " + " | ".join(f"`{impl}`" for impl in STRUCTURES) + " |",
        "|---|" + "---|" * len(STRUCTURES),
    ]
    for keys in SIZES:
        cells = [
            format_throughput(
                median_of(times, (impl, keys, ops_for(keys), insert_percent, order, queries))
            )
            for impl in STRUCTURES
        ]
        lines.append(f"| 10<sup>{len(str(keys)) - 1}</sup> | " + " | ".join(cells) + " |")
    return lines


def write_throughput(results: Path, plots: Path) -> None:
    read = load_reads(results / "read_size.csv")
    mix = load(results / "mix.csv")
    lines = [
        "# Throughput, million operations per second (median of 10 runs)",
        "",
        "“—” means the point was not measured.",
        "",
    ]
    for queries in QUERIES:
        lines += [f"## Reads, {QUERY_TITLES[queries]}", ""]
        lines += throughput_table(read, 0, "random", queries) + [""]
    for percent in INSERT_PERCENTS:
        lines += [f"## Mixed, {percent} % insertions", ""]
        lines += throughput_table(mix, percent, "random", "mixed") + [""]
    (plots / "throughput.md").write_text("\n".join(lines), encoding="utf-8")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", type=Path, default=Path("bench/results"))
    parser.add_argument("--plots-dir", type=Path, default=Path("bench/plots"))
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    arguments.plots_dir.mkdir(parents=True, exist_ok=True)
    for plot in (plot_build, plot_read_size, plot_read_order, plot_mix, plot_btree):
        plot(arguments.results_dir, arguments.plots_dir)
    write_throughput(arguments.results_dir, arguments.plots_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
