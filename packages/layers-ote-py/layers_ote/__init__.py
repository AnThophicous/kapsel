from __future__ import annotations

import json
import os
import pathlib
import subprocess
from typing import Dict


def read_proxy_env(cwd: str | None = None) -> dict[str, str]:
    root = pathlib.Path(cwd or os.getcwd())
    env_path = root / ".env"
    profile = "prod"
    if env_path.exists():
        for raw_line in env_path.read_text(encoding="utf-8").splitlines():
            line = raw_line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            if key.strip() == "OTE_PROFILE":
                profile = value.strip() or "prod"
                break
    return {"profile": profile, "path": str(env_path)}


def _resolve_binary_path(binary_path: str | None = None) -> str:
    if binary_path:
        return binary_path
    env_binary = os.environ.get("OTE_BINARY")
    if env_binary:
        return env_binary
    return "kapsel.exe" if os.name == "nt" else "kapsel"


def materialize(profile: str, cwd: str | None = None, binary_path: str | None = None) -> Dict[str, str]:
    root = pathlib.Path(cwd or os.getcwd())
    exe = _resolve_binary_path(binary_path)
    completed = subprocess.run(
        [exe, "bridge", "materialize", profile],
        cwd=str(root),
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        raise RuntimeError((completed.stderr or completed.stdout or "layers materialize failed").strip())
    payload = json.loads(completed.stdout)
    return dict(payload.get("env", {}))


def inject_into_process_env(cwd: str | None = None, profile: str | None = None, include_profile: bool = True, binary_path: str | None = None) -> Dict[str, str]:
    root = pathlib.Path(cwd or os.getcwd())
    proxy = read_proxy_env(str(root))
    active_profile = profile or proxy["profile"]
    env = materialize(active_profile, str(root), binary_path)
    for key, value in env.items():
        os.environ[key] = value
    if include_profile:
        os.environ["OTE_PROFILE"] = active_profile
    return env
