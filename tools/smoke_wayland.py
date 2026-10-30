#!/usr/bin/env python3
"""Run the actual AppImage on a native, headless Wayland compositor."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument("image", type=Path)
parser.add_argument("reports", type=Path)
args = parser.parse_args()
if os.geteuid() == 0:
    raise SystemExit("Run Wayland checks as a non-root user")
image = args.image.resolve()
args.reports.mkdir(parents=True, exist_ok=True)
state = Path(tempfile.mkdtemp(prefix="orbittlm-wayland-"))
runtime = state / "runtime"
runtime.mkdir(mode=0o700)
env = dict(os.environ, HOME=str(state), XDG_RUNTIME_DIR=str(runtime),
           XDG_DATA_HOME=str(state / "data"), XDG_CONFIG_HOME=str(state / "config"),
           XDG_CACHE_HOME=str(state / "cache"), XDG_SESSION_TYPE="wayland",
           WAYLAND_DISPLAY="wayland-orbittlm", QT_QPA_PLATFORM="wayland",
           QT_WAYLAND_SHELL_INTEGRATION="xdg-shell", WAYLAND_DEBUG="1",
           QT_PLUGIN_PATH="/invalid-host-qt/plugins",
           QT_QPA_PLATFORM_PLUGIN_PATH="/invalid-host-qt/platforms")
env.pop("DISPLAY", None)
compositor_env = dict(env)
compositor_env.pop("WAYLAND_DEBUG", None)
help_text = subprocess.run(["weston", "--backend=headless-backend.so", "--help"],
                           env=compositor_env, text=True, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, check=True).stdout
renderer = "--renderer=pixman" if "--renderer" in help_text else "--use-pixman"
with (args.reports / "weston.log").open("w", encoding="utf-8") as log:
    compositor = subprocess.Popen(["weston", "--backend=headless-backend.so", renderer,
                                   "--socket=wayland-orbittlm", "--idle-time=0",
                                   "--width=1280", "--height=800"],
                                  env=compositor_env, stdout=log, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 15
        while not (runtime / "wayland-orbittlm").exists():
            if compositor.poll() is not None or time.monotonic() > deadline:
                log.flush()
                raise RuntimeError((args.reports / "weston.log").read_text(encoding="utf-8"))
            time.sleep(0.05)
        data_dir = None
        for language in ("zh-CN", "en-US", "ja-JP", "auto"):
            selected = language if language != "auto" else "ja-JP"
            if data_dir:
                (data_dir / "app-settings.json").write_text(json.dumps({
                    "language": selected, "tcpHost": "127.0.0.1", "tcpPort": 9985
                }), encoding="utf-8")
            run_env = dict(env)
            if language == "auto":
                run_env.pop("QT_QPA_PLATFORM")
            report_file = args.reports.resolve() / (language + ".json")
            result = subprocess.run([str(image), "--appimage-extract-and-run", "--smoke-test",
                                     "--smoke-report", str(report_file)], env=run_env,
                                    text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=90)
            (args.reports / (language + ".log")).write_text(result.stdout, encoding="utf-8")
            if result.returncode:
                print(result.stdout, flush=True)
                raise SystemExit(f"Wayland {language}: returned {result.returncode}")
            report = json.loads(report_file.read_text(encoding="utf-8"))
            assert report["ok"] and report["platform"].startswith("wayland"), report
            assert report["language"] == selected and report["sqliteDriver"], report
            assert "xdg_toplevel" in result.stdout and ".commit(" in result.stdout, result.stdout
            assert "Could not find the Qt platform plugin" not in result.stdout, result.stdout
            data_dir = Path(report["dataDirectory"])
            assert data_dir.is_relative_to(state / "data"), report
            print(json.dumps(report), flush=True)
    finally:
        compositor.terminate()
        compositor.wait(timeout=10)
print("Native Wayland startup, rendered xdg-shell windows and automatic platform selection passed.")
