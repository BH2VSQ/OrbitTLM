#!/usr/bin/env python3
"""Exercise the packaged GUI on X11 without system Qt or a writable AppDir."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

args = argparse.ArgumentParser()
args.add_argument("image", type=Path)
args.add_argument("reports", type=Path)
args.add_argument("--readonly-appdir", type=Path)
options = args.parse_args()
image = options.image.resolve()
options.reports.mkdir(parents=True, exist_ok=True)
if os.geteuid() == 0:
    raise SystemExit("Run packaged GUI checks as a non-root user")
state = Path(tempfile.mkdtemp(prefix="orbittlm-runtime-"))
env = os.environ.copy()
env.update(HOME=str(state), XDG_DATA_HOME=str(state / "data"),
           XDG_CONFIG_HOME=str(state / "config"), XDG_CACHE_HOME=str(state / "cache"),
           QT_QPA_PLATFORM="xcb", QT_PLUGIN_PATH="/invalid-host-qt/plugins",
           QT_QPA_PLATFORM_PLUGIN_PATH="/invalid-host-qt/platforms",
           LD_LIBRARY_PATH="/invalid-host-qt/lib", QTDIR="/invalid-host-qt")
data_dir = None
for language in ("zh-CN", "en-US", "ja-JP"):
    if data_dir:
        (data_dir / "app-settings.json").write_text(json.dumps({
            "language": language, "tcpHost": "127.0.0.1", "tcpPort": 9985
        }), encoding="utf-8")
    report_file = options.reports.resolve() / (language + ".json")
    command = ["xvfb-run", "-a", "-s", "-screen 0 1280x800x24", str(image),
               "--appimage-extract-and-run", "--smoke-test", "--smoke-report", str(report_file)]
    result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=90)
    (options.reports / (language + ".log")).write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(f"{language}: packaged GUI returned {result.returncode}")
    report = json.loads(report_file.read_text(encoding="utf-8"))
    assert report["ok"] and report["language"] == language, report
    assert report["sqliteDriver"] and report["databaseCreated"] and report["historyCreated"], report
    assert report["platform"] == "xcb", report
    data_dir = Path(report["dataDirectory"])
    assert data_dir.is_relative_to(state / "data"), report
    assert not str(data_dir).startswith(report["applicationDirectory"]), report
    assert not (Path(report["applicationDirectory"]) / "data").exists(), report
    print(json.dumps(report), flush=True)
subprocess.run([str(image), "--appimage-extract-and-run", "--cli", "--help"],
               env=env, check=True, stdout=subprocess.DEVNULL, timeout=30)
print("Packaged X11 startup, language persistence, SQLite, writable storage and CLI passed.")
if options.readonly_appdir:
    appdir = options.readonly_appdir.resolve()
    assert not os.access(appdir / "usr/bin", os.W_OK), "AppDir must be read-only to the test user"
    library_env = dict(env, LD_LIBRARY_PATH=str(appdir / "usr/lib"))
    dependencies = subprocess.run(["ldd", str(appdir / "usr/bin/OrbitTLM")], env=library_env,
                                  text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=True)
    (options.reports / "dependencies.log").write_text(dependencies.stdout, encoding="utf-8")
    assert "not found" not in dependencies.stdout, dependencies.stdout
    report_file = options.reports.resolve() / "readonly-appdir.json"
    command = ["xvfb-run", "-a", str(appdir / "AppRun"), "--smoke-test", "--smoke-report", str(report_file)]
    result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=90)
    (options.reports / "readonly-appdir.log").write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        print(result.stdout, flush=True)
        raise SystemExit(f"Read-only AppDir returned {result.returncode}")
    report = json.loads(report_file.read_text(encoding="utf-8"))
    assert report["ok"] and Path(report["dataDirectory"]).is_relative_to(state / "data"), report
    assert not (appdir / "usr/bin/data").exists(), report
    print("Read-only AppDir startup passed.")
