# OTE Architecture

OTE is built around three separate responsibilities:

1. secret storage
2. policy control
3. sandboxed execution

The core rule is that secrets are never exposed to the caller. The runtime can resolve which secret profile is relevant, but it only returns the allowed variables, a redacted projection, or a sanitized execution plan.

## Stage 0 goals

- initialize a project folder
- create `ote-config.config`
- detect the local platform
- expose a small CLI
- keep memory use low

## Stage 1 goals

- split the build into reusable core and CLI layers
- validate config at startup
- create the runtime layout safely
- expose inspection commands
- keep the entrypoint small

## Stage 2 goals

- store secret payloads outside the public API surface
- keep secret names and keys identifier-like
- return only projections to untrusted tooling
- support a local manifest for MCP-style consumers
- protect payloads at rest through the strongest local backend available

## Stage 3 goals

- build execution plans from local policy
- reject blocked command syntax early
- keep the allowed working directory inside the configured allowlist
- filter environment variables before spawn
- use an internal broker instead of direct shell passthrough
- support execution modes that separate safety, user approval, and bypass of prompts

## Config file

`ote-config.config` is JSON even though it uses the `.config` suffix.

The file is user-owned and is not meant to be edited by AI tooling directly.

Recommended fields:

- `sandbox.enabled`
- `sandbox.allowed_paths`
- `sandbox.denied_paths`
- `sandbox.allowed_env`
- `runtime.default_shell`
- `runtime.cache_dir`

The current scaffold fills sensible defaults from the local root and validates the resulting policy before use.

## Sandbox model

The runtime should choose the strongest available isolation primitive for the host OS.

Windows:

- restricted token
- job object
- optional AppContainer
- named pipe broker

Linux:

- namespaces
- seccomp
- cgroups
- mount isolation

macOS:

- seatbelt-style sandboxing
- process spawning with restricted environment
- brokered file access

The abstraction layer should keep the execution policy separate from the platform backend.

## Secret access model

The AI requests a secret profile by name or intent.
OTE resolves the profile locally.
OTE returns only:

- allowed variable names
- permitted metadata
- execution context

OTE does not return raw secret values through CLI, API, or MCP.

The current Stage 2 scaffold exposes redacted projections through `ote secret describe` and `ote api secrets`, while the internal vault path is kept separate from config and status commands.

The current Stage 3 scaffold adds `ote exec plan` and `ote exec run`, both of which go through `ProcessBroker` and the active sandbox backend.

Windows defaults to `cmd` for the main execution path; `powershell` remains available as an explicit shell option.

Execution modes:

- `safe`: only allowlisted read-only or inspection-style commands
- `permission`: validate the plan, then ask the user before executing
- `bypass`: skip the prompt but still enforce path and syntax policy

## MCP Model

The MCP server is stdio-based and local-only.

It exposes:

- `secret.list`
- `secret.describe`
- `secret.add`
- `exec.plan`
- `exec.run`
- `status`
- `paths`
- `config.show`

It also exposes read-only resources for status, paths, config, manifest, and root.

The server is intentionally narrow:

- no raw secret payloads
- no direct config mutation from the agent
- no shell passthrough around the broker
- no bypass of local policy checks

## Next stage

Stage 4 should finish platform backends, output capture, cross-platform packaging, and release automation.
