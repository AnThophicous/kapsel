# OTE

**One Time Execution**

Stop exposing your `.env` to AI tools.
OTE is a local secret vault, command broker, MCP server, and `.env` replacement built for AI-assisted development.

It keeps secrets on your machine, keeps the agent out of the vault, and still gives the agent enough structure to do useful work.

## Why OTE Exists

Still struggling with AI reading your `.env`?

That is the problem OTE is built to solve.

With OTE, the agent can:

- discover redacted secret metadata
- request a safe execution plan
- run commands through a local policy layer
- use MCP without seeing raw secret values

With OTE, the agent cannot:

- read raw SKs
- bypass the local policy model
- edit the protected config file
- turn your `.env` into a leak

## What OTE Replaces

OTE is designed to replace the fragile parts of classic `.env` workflows:

- exposed environment files
- ad hoc secret copying
- unsafe shell passthrough
- agent-visible secrets
- manual MCP config setup

## Core Value

OTE is built for developers who want:

- AI-friendly tooling
- local-first secret storage
- low overhead
- fast command execution
- clean DX
- stronger trust boundaries

## Main Features

- local protected secret storage
- redacted secret projections
- policy-checked command execution
- safe / permission / bypass execution modes
- stdio MCP server
- MCP client installer for common tools
- Layers bridge for Node.js, TypeScript, and Python
- `.env` migration into managed local secrets
- automatic config and path helpers

## Common Use Cases

OTE fits teams searching for:

- AI env security
- dotenv replacement
- secret exposure prevention
- local MCP server
- Claude MCP install
- Cursor MCP config
- VS Code MCP config
- Windsurf MCP config
- OpenAI Codex MCP setup
- Node.js secret bridge
- Python secret bridge

## How It Works

1. run `ote --setup`
2. migrate an existing `.env` with `ote --migration`
3. let OTE rewrite the `.env` into a proxy
4. use `ote bridge env` or `ote bridge run` when the process needs the managed values
5. use `ote mcp install <target>` to wire a local client
6. keep raw secrets inside OTE only

The on-disk `.env` becomes a small proxy like this:

```env
# Managed by OTE
OTE_PROFILE=prod
```

## MCP

OTE ships a local stdio MCP server so agents can use it as tooling without getting direct access to secret values.

Key commands:

- `ote mcp manifest`
- `ote mcp config`
- `ote mcp install <target>`
- `ote mcp install --print`
- `ote mcp install --config <path>`
- `ote mcp doctor`
- `ote mcp serve`

Supported install targets:

- `claude`
- `cursor`
- `vscode`
- `windsurf`
- `custom`

`ote mcp install` only updates `mcpServers.ote`.
It merges safely and leaves existing client config intact.

## Layers

Layers is the bridge that turns `.env` into a managed OTE-backed proxy for trusted application runtimes.

Use it when you want:

- `process.env` to keep working in Node.js
- `os.environ` to keep working in Python
- the real source of truth to stay inside OTE

Useful commands:

- `ote bridge manifest [profile]`
- `ote bridge materialize [profile]`
- `ote bridge env [profile]`
- `ote bridge run [--profile <name>] [shell] <command>`

## Security Model

OTE is designed around a single rule:

**the agent never receives raw secret values**

The broker controls:

- command policy
- path policy
- execution mode
- environment filtering

The vault controls:

- protected secret storage
- secret metadata
- redacted projection output

## Quick Start

```bash
ote --setup
ote --doctor
ote --migration
ote mcp doctor
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Search Keywords

AI env security, dotenv replacement, secret exposure prevention, local secret store, local MCP server, AI tooling security, MCP install, Claude Desktop MCP, Cursor MCP, VS Code MCP, Windsurf MCP, Codex CLI MCP, Node.js secret bridge, Python secret bridge, low overhead secret broker.

## International Terms

- Portuguese: segurança de `.env` para IA, substituto do dotenv, MCP local, proteção de segredo local
- Spanish: seguridad de `.env` para IA, reemplazo de dotenv, servidor MCP local
- French: sécurité `.env` pour IA, remplacement de dotenv, serveur MCP local

## Docs

- [docs/architecture.md](docs/architecture.md)
- [docs/security.md](docs/security.md)
- [docs/mcp.md](docs/mcp.md)
- [docs/layers.md](docs/layers.md)
- [docs/agent-integration.md](docs/agent-integration.md)
- [docs/release.md](docs/release.md)
- [docs/publishing.md](docs/publishing.md)
