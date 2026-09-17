# OIViewer startup benchmark

`benchmark_viewer_startup.py` is a Python 3.9+ script for Windows and Linux. It requires no Python packages, PowerShell, profiler, or administrator privileges. OIViewer itself still needs a working graphics/display environment.

The metric is **process launch to successful renderer initialization**, detected from the existing `[Renderer] Selected ...` message. It includes process creation and graphics initialization. It does **not** measure the first presented frame, completed image loading, or physical display latency. The existing `check_viewer_startup.py` remains a separate process-survival smoke check.

## Windows

```powershell
python Tests/Scripts/benchmark_viewer_startup.py publish/bin/OIViewer.exe --renderers D3D11 Vulkan --adapter-name NVIDIA --runs 7 --output build/startup-windows.json
```

## Linux

```bash
python3 Tests/Scripts/benchmark_viewer_startup.py build/linux-clang-release/bin/OIViewer --renderers Vulkan GL --runs 7 --output build/startup-linux.json
```

Use only renderers compiled into the selected binary. Without `--renderers`, the script selects D3D11 on Windows and Vulkan on Linux. Every launch uses an explicit renderer option, preventing input forwarding to an existing viewer instance.

Add `--input path/to/image-or-folder` for a particular launch scenario. The readiness endpoint remains renderer initialization; image decoding may still be in progress. `--adapter-name` and `--adapter-index` are mutually exclusive and apply to D3D11/Vulkan. For GL, use the OS/driver selection mechanism, such as Mesa's `DRI_PRIME` environment variable.

The script starts a fresh process for every sample, alternates renderer order, and terminates only its own viewer process after recording readiness. It does not wait for normal application shutdown. A missing readiness message, early process exit, unexpected renderer, or timeout fails the run; partial measurements are retained when `--output` is supplied. Existing output files are refused rather than overwritten.

Results include individual samples, the selected GPU/acceleration from the readiness message, executable SHA-256, median, mean, minimum, maximum, standard deviation, and coefficient of variation. Fewer than three samples or variation above 5% is flagged as insufficient repeatability. OS and driver caches are not reset; use the same executable, input, GPU, and machine conditions when comparing runs.

Keep the display session and background workload stable. Results from different operating systems describe the whole launch environment, not only the renderer. Measuring first-image presentation consistently on native Wayland and Windows would require an additional application readiness signal.

## Script tests

```text
python -m unittest discover -s Tests/Scripts -p test_benchmark_viewer_startup.py
```

The tests use temporary child processes to check the readiness endpoint, large diagnostic output, early failure, timeout cleanup, unexpected renderer selection, and statistics. They do not launch OIViewer.