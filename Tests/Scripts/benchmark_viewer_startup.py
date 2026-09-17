"""Measure OIViewer launch-to-renderer-ready startup on Windows and Linux.

Requires Python 3.9+ and an OIViewer build that logs '[Renderer] Selected ...'.
This measures successful renderer initialization, not first-image presentation.
Each sample launches and then terminates its own viewer process.
"""

import argparse
from collections import deque
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import queue
import re
import statistics
import subprocess
import sys
import threading
import time


READY_PATTERN = re.compile(
    r"^\[Renderer\] Selected (D3D11|Vulkan|GL) adapter .* \[(Hardware|Unknown|Software)\]$"
)


class StartupError(RuntimeError):
    """Startup failed, timed out, or selected an unexpected renderer."""


def measure_startup(command, cwd, renderer, timeout):
    """Time process creation through receipt of the existing renderer-ready log.

    Drain output on a reader thread so startup cannot block on a full pipe on
    either platform. Only the process created here is stopped, including on
    timeout or interruption. Cleanup time is outside the measurement.
    """
    messages = queue.Queue()
    tail = deque(maxlen=40)
    process = None
    reader = None
    result = None

    def read_output():
        try:
            for line in process.stdout:
                messages.put((time.perf_counter_ns(), line.rstrip("\r\n")))
        finally:
            messages.put((time.perf_counter_ns(), None))

    start_ns = time.perf_counter_ns()
    try:
        process = subprocess.Popen(
            command,
            cwd=cwd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            # Suppress a separate console on Windows; the viewer's GUI is unaffected.
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )
        reader = threading.Thread(target=read_output, daemon=True)
        reader.start()
        deadline_ns = start_ns + int(timeout * 1_000_000_000)
        while result is None:
            remaining = (deadline_ns - time.perf_counter_ns()) / 1_000_000_000
            if remaining <= 0:
                raise StartupError(f"No renderer-ready message within {timeout:g} seconds")
            try:
                timestamp_ns, line = messages.get(timeout=remaining)
            except queue.Empty:
                raise StartupError(f"No renderer-ready message within {timeout:g} seconds") from None
            if timestamp_ns > deadline_ns:
                raise StartupError(f"Renderer-ready message arrived after the {timeout:g}-second timeout")
            if line is None:
                raise StartupError(
                    f"Output closed before renderer initialization (exit code {process.poll()})"
                )
            tail.append(line)
            match = READY_PATTERN.fullmatch(line)
            if match:
                if match[1] != renderer:
                    raise StartupError(f"Requested {renderer}, but the viewer selected {match[1]}")
                if process.poll() is not None:
                    raise StartupError(f"Viewer exited during startup (exit code {process.returncode})")
                result = {
                    "pid": process.pid,
                    "elapsed_ms": (timestamp_ns - start_ns) / 1_000_000,
                    "ready_message": line,
                    "acceleration": match[2],
                }
    except StartupError as error:
        diagnostic = "\n".join(tail)
        raise StartupError(f"{error}\n{diagnostic}".rstrip()) from error
    finally:
        if process is not None:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            if reader is not None:
                reader.join(timeout=1)
            if process.stdout is not None and (reader is None or not reader.is_alive()):
                process.stdout.close()
            if result is not None:
                result["cleanup_exit_code"] = process.returncode
                result["output_tail"] = list(tail)
    return result


def summarize(samples):
    values = [sample["elapsed_ms"] for sample in samples]
    mean = statistics.mean(values)
    deviation = statistics.stdev(values) if len(values) > 1 else 0.0
    cv = deviation / mean * 100 if mean else 0.0
    # A single observation cannot establish repeatability.
    return {
        "runs": len(values),
        "median_ms": statistics.median(values),
        "mean_ms": mean,
        "min_ms": min(values),
        "max_ms": max(values),
        "stdev_ms": deviation,
        "cv_percent": cv,
        "repeatability_within_5_percent": len(values) >= 3 and cv <= 5.0,
    }


def positive_integer(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def nonnegative_integer(value):
    number = int(value)
    if number < 0:
        raise argparse.ArgumentTypeError("must be nonnegative")
    return number


def positive_seconds(value):
    number = float(value)
    if not 0 < number < float("inf"):
        raise argparse.ArgumentTypeError("must be a finite positive number")
    return number


def write_report(path, report):
    if path is not None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8", newline="\r\n") as stream:
            json.dump(report, stream, indent=2, ensure_ascii=False)
            stream.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--input", type=Path, help="optional image or folder")
    parser.add_argument(
        "--renderers", nargs="+", choices=("D3D11", "Vulkan", "GL"),
        default=["D3D11" if sys.platform == "win32" else "Vulkan"],
        help="renderers to measure; default: D3D11 on Windows, Vulkan on Linux",
    )
    adapter = parser.add_mutually_exclusive_group()
    adapter.add_argument("--adapter-name")
    adapter.add_argument("--adapter-index", type=nonnegative_integer)
    parser.add_argument("--runs", type=positive_integer, default=7)
    parser.add_argument("--timeout", type=positive_seconds, default=30)
    parser.add_argument("--pause", type=positive_seconds, default=1, help="seconds between launches")
    parser.add_argument("--output", type=Path, help="optional JSON report; must not already exist")
    args = parser.parse_args(argv)
    if sys.platform not in ("win32", "linux"):
        parser.error("only Windows and Linux are supported")
    try:
        executable = args.executable.resolve(strict=True)
        input_path = args.input.resolve(strict=True) if args.input else None
    except OSError as error:
        parser.error(str(error))
    if not executable.is_file():
        parser.error("executable must be a file")
    if len(set(args.renderers)) != len(args.renderers):
        parser.error("specify each renderer once")
    if "GL" in args.renderers and (args.adapter_name is not None or args.adapter_index is not None):
        parser.error("OIViewer GL does not support explicit adapter options; use OS/driver selection")
    output = args.output.resolve() if args.output else None
    if output is not None and output.exists():
        parser.error("output already exists; choose a new report path")

    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    report = {
        "schema_version": 1,
        "metric": "process_launch_to_renderer_ready_log_ms",
        "endpoint": "successful renderer initialization; excludes first-image presentation",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(),
        "python_version": platform.python_version(),
        "executable": str(executable),
        "executable_sha256": digest,
        "input": str(input_path) if input_path else None,
        "requested_runs": args.runs,
        "cache_policy": "fresh process each run; OS and driver caches are not reset",
        "samples": [],
        "summary": {},
        "status": "running",
    }
    exit_code = 0
    try:
        for round_index in range(args.runs):
            # Alternate ordering to reduce systematic warm-cache/order bias.
            order = args.renderers if round_index % 2 == 0 else list(reversed(args.renderers))
            for renderer in order:
                # Explicit renderer selection also prevents forwarding an input to an existing viewer.
                command = [str(executable), "--renderer", renderer]
                if args.adapter_name is not None:
                    command += ["--adapter-name", args.adapter_name]
                if args.adapter_index is not None:
                    command += ["--adapter-index", str(args.adapter_index)]
                if input_path is not None:
                    command += ["--", str(input_path)]
                sample = measure_startup(command, executable.parent, renderer, args.timeout)
                sample.update(renderer=renderer, run=round_index + 1, command=command)
                report["samples"].append(sample)
                write_report(output, report)
                print(f"{renderer:7} run {round_index + 1}/{args.runs}: {sample['elapsed_ms']:.2f} ms (renderer-ready)", flush=True)
                time.sleep(args.pause)
        if hashlib.sha256(executable.read_bytes()).hexdigest() != digest:
            raise StartupError("Executable changed during the benchmark; discard this comparison")
        report["status"] = "complete"
    except KeyboardInterrupt:
        report.update(status="interrupted", error="Interrupted by user")
        exit_code = 130
    except (OSError, StartupError) as error:
        report.update(status="failed", error=str(error))
        print(f"ERROR: {error}", file=sys.stderr)
        exit_code = 1
    finally:
        for renderer in args.renderers:
            samples = [sample for sample in report["samples"] if sample["renderer"] == renderer]
            if samples:
                report["summary"][renderer] = summarize(samples)
        write_report(output, report)

    print("\nRenderer  Runs  Median ms    Min ms    Max ms      CV")
    for renderer, stats in report["summary"].items():
        print(f"{renderer:8} {stats['runs']:4} {stats['median_ms']:10.2f} {stats['min_ms']:9.2f} {stats['max_ms']:9.2f} {stats['cv_percent']:6.1f}%")
        if not stats["repeatability_within_5_percent"]:
            print("  Repeatability not established: avoid interpreting small timing differences.")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())