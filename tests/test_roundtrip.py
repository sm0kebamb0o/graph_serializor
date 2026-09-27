from collections import Counter
from pathlib import Path
import subprocess
import tempfile


PROJECT_ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = PROJECT_ROOT / "run"
MAX_VERTEX_ID = 2**32 - 1
MAX_WEIGHT = 2**8 - 1

_built = False


def ensure_built():
    global _built
    if not _built:
        subprocess.run(["make"], cwd=PROJECT_ROOT, check=True)
        _built = True


def read_edges(path):
    edges = Counter()
    with path.open("r", encoding="ascii", newline="") as stream:
        for line_number, line in enumerate(stream, start=1):
            assert line.endswith("\n"), f"line {line_number} does not end with LF"
            line = line[:-1]
            assert "\r" not in line, f"line {line_number} contains CR"

            columns = line.split("\t")
            assert len(columns) == 3, f"line {line_number} must contain exactly three columns"
            assert all(column.isascii() and column.isdecimal() for column in columns), (
                f"line {line_number} contains a non-integer value"
            )

            first, second, weight = map(int, columns)
            assert 0 <= first <= MAX_VERTEX_ID
            assert 0 <= second <= MAX_VERTEX_ID
            assert 0 <= weight <= MAX_WEIGHT
            edges[min(first, second), max(first, second), weight] += 1
    return edges


def assert_roundtrip(input_path, directory):
    ensure_built()
    binary_path = directory / "graph.bin"
    output_path = directory / "output.tsv"

    subprocess.run(
        [EXECUTABLE, "-s", "-i", input_path, "-o", binary_path],
        cwd=PROJECT_ROOT,
        check=True,
    )
    subprocess.run(
        [EXECUTABLE, "-d", "-i", binary_path, "-o", output_path],
        cwd=PROJECT_ROOT,
        check=True,
    )

    assert binary_path.stat().st_size > 0
    assert read_edges(output_path) == read_edges(input_path)


def test_roundtrip_boundary_values_and_loop():
    with tempfile.TemporaryDirectory() as temporary_directory:
        directory = Path(temporary_directory)
        input_path = directory / "input.tsv"
        input_path.write_text(
            "0\t4294967295\t0\n"
            "4294967295\t17\t255\n"
            "42\t42\t128\n"
            "5\t4\t1\n",
            encoding="ascii",
        )
        assert_roundtrip(input_path, directory)


def test_roundtrip_small_example():
    with tempfile.TemporaryDirectory() as temporary_directory:
        assert_roundtrip(PROJECT_ROOT / "small_example.tsv", Path(temporary_directory))


if __name__ == "__main__":
    test_roundtrip_boundary_values_and_loop()
    test_roundtrip_small_example()
