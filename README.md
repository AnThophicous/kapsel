# OTE

OTE means One Time Execution.

OTE is a local secret store and command broker built for AI-assisted tooling.
It is designed to replace the unsafe parts of `.env` workflows without ever exposing raw SKs to the agent.

## What OTE Does

- stores secrets locally in protected form
- returns only redacted secret projections to tooling
- executes commands through a policy layer, not direct shell passthrough
- exposes a local MCP server for agent integrations
- keeps config, runtime state, and vault data inside the project root

## Security Model

The core rule is simple: the agent never receives the raw secret value.

OTE can:

- list secret metadata
- describe allowed keys and tags
- choose a command plan
- run a command through the broker

OTE cannot:

- expose raw SKs through CLI, API, or MCP
- skip path policy even in `bypass`
- let an agent edit `ote-config.config`

## Execution Modes

- `safe`: allowlisted commands only
- `permission`: validate first, then ask the user
- `bypass`: skip the prompt, but keep policy checks

## Core Commands

- `ote --setup`
- `ote --status`
- `ote --doctor`
- `ote --validate`
- `ote --paths`
- `ote config show`
- `ote secret list`
- `ote secret describe <name>`
- `ote secret add <name> [--tag <tag>] KEY=VALUE...`
- `ote api manifest`
- `ote api secrets`
- `ote exec plan <command>`
- `ote exec run <command>`
- `ote mcp manifest`
- `ote mcp serve`

## MCP

OTE ships a stdio MCP server so agents can use it as local tooling.

- `ote mcp manifest` returns the integration manifest
- `ote mcp serve` starts the stdio server
- tools are redacted by design
- resources expose status, paths, config, and manifest snapshots

See [docs/mcp.md](docs/mcp.md) and [docs/agent-integration.md](docs/agent-integration.md).

## Layout

- `ote-config.example.config` is the tracked example policy file
- `ote-config.config` is the user-owned policy file
- `.ote/` stores the local runtime state
- `.ote/secrets/records/` stores protected secret records
- `.ote/cache/` stores transient runtime cache
- `.ote/logs/` stores logs

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Docs

- [docs/architecture.md](docs/architecture.md)
- [docs/security.md](docs/security.md)
- [docs/mcp.md](docs/mcp.md)
- [docs/agent-integration.md](docs/agent-integration.md)
- [docs/release.md](docs/release.md)
