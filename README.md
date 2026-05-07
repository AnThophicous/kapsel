# OTE

**One Time Execution**

OTE is a local secret vault, execution broker, MCP server, and `.env` replacement for AI-assisted development.

It is built for teams that want strong local security without losing DX.

The goal is simple:

- keep raw secrets on the machine
- keep the agent out of the vault
- keep commands inside a policy layer
- make MCP setup predictable
- make `.env` workflows safer and easier to automate

## Problem

Classic `.env` workflows break down when AI tools enter the picture.

The usual failure modes are:

- the agent reads the file directly
- secrets get copied into prompts or logs
- command execution becomes raw shell passthrough
- every MCP client needs manual setup
- env handling is inconsistent across Node.js, Python, and CLI tools

OTE exists to replace that mess with a local, structured boundary.

## What OTE Does

OTE gives you:

- protected local secret storage
- redacted secret projections
- a local execution broker
- policy modes for safer automation
- a stdio MCP server
- MCP installers for common clients
- a Layers bridge for `.env` migration
- low-overhead runtime behavior
- cross-platform packaging

OTE does not give the agent:

- raw SKs
- decrypted vault payloads
- direct config file writes
- path-policy bypass

## Why OTE Is Different

OTE is not just another dotenv helper.
It is closer to a local trust boundary for AI tooling.

That means:

- the agent can ask for what it needs
- the agent can inspect metadata
- the agent can request execution through the broker
- the agent cannot grab the secret itself

This is useful when you want:

- AI env security
- secret exposure prevention
- dotenv replacement
- local MCP setup
- safer command execution
- consistent `.env` behavior across runtimes

## Core Concepts

### 1. Vault

Secrets are stored locally in protected form under the project root.

The vault only exposes:

- secret name
- tags
- allowed keys
- protector name

### 2. Broker

Commands do not go straight to the shell.
They go through a broker that validates:

- shell choice
- working directory
- allowed paths
- denied paths
- allowed environment
- execution mode

### 3. MCP

OTE ships a local stdio MCP server.
Agents can use it as tooling without seeing raw secret values.

### 4. Layers

Layers turns a normal `.env` into a proxy for OTE.

That lets application code keep using:

- `process.env` in Node.js
- `os.environ` in Python

while the real values stay inside OTE.

## Execution Modes

OTE supports three execution modes:

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

## Quick Start

```bash
ote --setup
ote --doctor
ote --migration
ote mcp doctor
```

If you already have a project `.env`:

```bash
ote --migration
```

That migrates the file into OTE-managed storage and rewrites the `.env` into a proxy.

## `.env` Proxy Model

After migration, the `.env` becomes a small proxy:

```env
# Managed by OTE
OTE_PROFILE=prod
```

The source of truth moves into OTE.
The file stays useful to the application runtime.

## Layers Workflow

Use Layers when you want OTE to sit between the runtime and the old `.env` workflow.

Helpful commands:

```bash
ote bridge manifest [profile]
ote bridge materialize [profile]
ote bridge env [profile]
ote bridge run [--profile <name>] [shell] <command>
```

### `ote bridge manifest`

Prints the bridge metadata for a profile.

### `ote bridge materialize`

Returns the resolved environment values as a structured payload.

### `ote bridge env`

Prints shell-friendly key/value lines for the managed environment.

### `ote bridge run`

Runs a child process with the managed environment already applied.

Examples:

```bash
ote bridge run --profile prod node server.js
ote bridge run --profile prod python app.py
ote bridge run --profile prod echo %API_KEY%
```

If a matching secret does not exist yet, OTE can migrate the local `.env` on first use.

## MCP

OTE ships a local stdio MCP server so agents can use it as tooling without direct secret exposure.

Key commands:

```bash
ote mcp manifest
ote mcp config
ote mcp install <target>
ote mcp install --print
ote mcp install --config <path>
ote mcp doctor
ote mcp serve
```

### MCP Features

- manifest generation
- secret projection tools
- path and status resources
- local execution broker access
- client config generation
- merge-safe install into existing config

### Supported Install Targets

- `claude`
- `cursor`
- `vscode`
- `windsurf`
- `custom`

### MCP Install Behavior

`ote mcp install` only updates `mcpServers.ote`.

It does not overwrite the rest of the file.

Examples:

```bash
ote mcp install claude
ote mcp install cursor
ote mcp install vscode
ote mcp install windsurf
ote mcp install --config ./mcp.json
ote mcp install --print
```

### Example MCP JSON

```json
{
  "mcpServers": {
    "ote": {
      "command": "C:\\OTE\\build-gcc\\ote.exe",
      "args": ["mcp", "serve"],
      "cwd": "C:\\OTE"
    }
  }
}
```

### MCP Doctor

`ote mcp doctor` validates:

- executable path
- project root
- config validity
- manifest generation
- broker availability

## Common Client Setup

### Claude Desktop

Use `ote mcp install claude` when you want OTE to write the local config for you.

### Cursor

Use `ote mcp install cursor` or `ote mcp install --config ~/.cursor/mcp.json`.

### VS Code

Use `ote mcp install vscode`.

### Windsurf

Use `ote mcp install windsurf`.

### Custom

Use `ote mcp install --config <path>`.

## Secret Handling

Secrets are protected locally and never exposed as raw values to the agent.

The vault exposes redacted projections only.

Available metadata:

- secret name
- tags
- allowed keys
- protector name

Unavailable data:

- raw secret values
- decrypted payloads
- direct config mutation by the agent

## Security Model

OTE is built around a single rule:

**the agent never receives the raw secret value**

The broker enforces:

- allowed paths
- denied paths
- shell restrictions
- blocked command syntax
- destructive verb checks
- execution mode policy

The config is user-owned.
The agent cannot edit `ote-config.config` directly.

## Project Layout

- `ote-config.example.config` is the tracked example policy file
- `ote-config.config` is the user-owned policy file
- `.ote/` stores runtime state
- `.ote/secrets/records/` stores secret records
- `.ote/cache/` stores transient cache
- `.ote/logs/` stores logs
- `.ote/layers/` stores bridge manifests

## Core Commands

```bash
ote --setup
ote --status
ote --doctor
ote --validate
ote --paths
ote --putpath
ote update
ote config show
ote secret list
ote secret describe <name>
ote secret add <name> [--tag <tag>] KEY=VALUE...
ote bridge manifest [profile]
ote bridge materialize [profile]
ote bridge env [profile]
ote bridge run [--profile <name>] [shell] <command>
ote api manifest
ote api secrets
ote exec plan <command>
ote exec run <command>
ote mcp manifest
ote mcp config
ote mcp install <target>
ote mcp install --print
ote mcp install --config <path>
ote mcp doctor
ote mcp serve
```

## Examples

### Setup a project

```bash
cd C:\OTE
ote --setup
ote --doctor
```

### Add a secret

```bash
ote secret add api-prod API_KEY=alpha123 DB_URL=postgres://local --tag prod --tag api
ote secret list
ote secret describe api-prod
```

### Inspect the active config

```bash
ote config show
ote --paths
ote --status
```

### Plan a command

```bash
ote exec plan echo hello
ote exec plan powershell Get-ChildItem
```

### Run a command

```bash
ote exec run echo hello
ote exec run cmd echo hello
```

### Migrate a `.env`

```bash
ote --migration
```

### Use the bridge

```bash
ote bridge env
ote bridge run --profile prod node server.js
```

### Generate MCP config

```bash
ote mcp config
ote mcp doctor
```

### Install MCP config

```bash
ote mcp install claude
ote mcp install cursor
ote mcp install vscode
ote mcp install windsurf
ote mcp install --config ./mcp.json
```

### Put OTE on PATH

```bash
ote --putpath
```

## Update Flow

```bash
ote update
```

The update command:

- checks the latest GitHub release
- compares it with the local version
- downloads the matching asset into the cache
- stages the extracted package for the next step

## Troubleshooting

### "DLL not found" on Windows

If Windows says a runtime DLL is missing, use the packaged release build or rebuild from the current tree.

Current Windows packaging includes the MinGW runtime DLL that the GNU build needs.

### "mcp doctor failed"

Check:

- project root exists
- `ote.exe` path resolves correctly
- config file is valid
- the release asset matches the current platform

### "bridge env failed: secret not found"

Run `ote --migration` once in that project, or create the protected secret first.

### "config invalid"

Run:

```bash
ote --validate
```

Then fix the reported issue in the user-owned config.

## Build

### Local Build

```bash
cmake -S . -B build
cmake --build build
cmake --build build --target package
```

### Windows Build Notes

On Windows GNU builds, OTE statically links the C++ runtime pieces it can and packages the required MinGW runtime DLL when needed.

That is what keeps the downloaded zip from failing on launch because of a missing runtime DLL.

## Cross-Platform Release

The release workflow targets:

- Windows
- Linux
- macOS

GitHub Actions builds the matrix and uploads release assets.

## What GitHub Actions Does

The release workflow:

- checks out the repo
- configures CMake
- builds the project
- packages the artifacts
- uploads the per-platform assets
- publishes the GitHub release

## FAQ

### Is OTE a secret manager?

Yes. It is a local secret store designed to keep raw values off the agent surface.

### Is OTE an `.env` replacement?

Yes. It is designed to replace the fragile parts of dotenv workflows.

### Does OTE work with MCP?

Yes. OTE ships a local MCP server and client installers.

### Does OTE support Node.js and Python?

Yes. Layers is built for both runtime styles.

### Can the agent ever see the raw secret?

No. That is the boundary OTE is built to preserve.

### Can I use OTE on Windows, Linux, and macOS?

Yes.

## Search Keywords

AI env security, dotenv replacement, secret exposure prevention, local secret store, local MCP server, MCP install, Claude Desktop MCP, Cursor MCP, VS Code MCP, Windsurf MCP, Codex CLI MCP, Node.js secret bridge, Python secret bridge, low overhead secret broker, environment proxy, protected secret vault, local policy layer.

## International Terms

- Portuguese: seguranca de `.env` para IA, substituto do dotenv, MCP local, protecao de segredo local
- Spanish: seguridad de `.env` para IA, reemplazo de dotenv, servidor MCP local
- French: securite `.env` pour IA, remplacement de dotenv, serveur MCP local

## Docs

The `docs/` folder contains deeper notes, but this README is the main user-facing document.

- [docs/architecture.md](docs/architecture.md)
- [docs/security.md](docs/security.md)
- [docs/mcp.md](docs/mcp.md)
- [docs/layers.md](docs/layers.md)
- [docs/agent-integration.md](docs/agent-integration.md)
- [docs/release.md](docs/release.md)
- [docs/publishing.md](docs/publishing.md)

## Release Status

OTE 1.0.2 is the current release line for this tree.

The current packaged Windows build includes the runtime DLL needed for the GNU build path.

## Final Summary

OTE is a local boundary for AI-assisted development:

- secrets stay local
- commands stay policy-checked
- `.env` becomes a proxy
- MCP setup becomes installable
- runtime bridges stay simple
- the agent stays useful without seeing the vault
