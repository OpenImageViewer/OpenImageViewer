"""Portable process/protocol tests: python -m unittest discover -s Tests/Scripts -p test_benchmark_viewer_startup.py"""

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import benchmark_viewer_startup as benchmark


class StartupBenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.processes = []
        self.real_popen = subprocess.Popen

    def spawn(self, *args, **kwargs):
        process = self.real_popen(*args, **kwargs)
        self.processes.append(process)
        return process

    def measure(self, source, renderer="Vulkan", timeout=3):
        with patch.object(benchmark.subprocess, "Popen", side_effect=self.spawn):
            try:
                return benchmark.measure_startup(
                    [sys.executable, "-u", "-c", source], Path(self.directory.name), renderer, timeout
                )
            finally:
                self.assertTrue(self.processes)
                for process in self.processes:
                    self.assertIsNotNone(process.poll(), "The launched process was left running")

    def test_waits_for_renderer_message_and_drains_stderr(self):
        result = self.measure(
            "import sys,time; sys.stderr.reconfigure(encoding='utf-8'); sys.stderr.write('diagnostic\\n'*20000); "
            "time.sleep(.10); "
            "print('[Renderer] Selected Vulkan adapter 0 (GPU \\u00e9) [Hardware]', file=sys.stderr, flush=True); "
            "time.sleep(30)"
        )
        self.assertGreaterEqual(result["elapsed_ms"], 90)
        self.assertLess(result["elapsed_ms"], 3000)
        self.assertEqual(result["acceleration"], "Hardware")
        self.assertIn("GPU é", result["ready_message"])
        self.assertLessEqual(len(result["output_tail"]), 40)

    def test_timeout_stops_the_launched_process(self):
        with self.assertRaisesRegex(benchmark.StartupError, "within"):
            self.measure("import time; time.sleep(30)", timeout=.15)

    def test_early_exit_is_not_success(self):
        with self.assertRaisesRegex(benchmark.StartupError, "Output closed before"):
            self.measure("import sys; print('startup failed',flush=True); sys.exit(7)")

    def test_wrong_renderer_is_not_success(self):
        with self.assertRaisesRegex(benchmark.StartupError, "Requested Vulkan"):
            self.measure(
                "import time; print('[Renderer] Selected D3D11 adapter 0 (GPU) [Hardware]',flush=True); "
                "time.sleep(30)"
            )

    def test_partial_initialization_log_is_not_readiness(self):
        with self.assertRaisesRegex(benchmark.StartupError, "Output closed before"):
            self.measure("print('[VK] Selected GPU: Test GPU',flush=True)")

    def test_statistics_preserve_noise_and_single_sample_uncertainty(self):
        stable = benchmark.summarize([{"elapsed_ms": value} for value in (99, 100, 101)])
        self.assertEqual(stable["median_ms"], 100)
        self.assertTrue(stable["repeatability_within_5_percent"])
        noisy = benchmark.summarize([{"elapsed_ms": value} for value in (100, 200, 300)])
        self.assertFalse(noisy["repeatability_within_5_percent"])
        self.assertFalse(benchmark.summarize([{"elapsed_ms": 100}])["repeatability_within_5_percent"])


if __name__ == "__main__":
    unittest.main()