# layers-ote

Python bridge for OTE-backed `.env` proxy workflows.

## Install

```bash
pip install .
```

## Use

```py
from layers_ote import inject_into_process_env

inject_into_process_env()
```

The package reads the local proxy `.env`, resolves `OTE_PROFILE`, and asks OTE to materialize the trusted environment into the current Python process.

If `ote` is not on `PATH`, pass `binary_path` or set `OTE_BINARY`.
