# OTE Security

OTE is built around a narrow trust boundary:

- the user owns the local config
- the agent can request work
- the agent never receives raw SKs
- the broker decides whether a command can run

## Secret Handling

Secrets are stored locally in protected form.
Only redacted projections are available to CLI, API, and MCP consumers.

Available data:

- secret name
- tags
- allowed keys
- protector name

Unavailable data:

- raw secret material
- decrypted payloads
- config file mutation by the agent

## Why This Matters

The most dangerous failure mode is not a leaked vault file.
It is an agent that sees a key and then uses it outside the intended path.

OTE prevents that by design:

- secrets stay local
- the runtime only returns projections
- execution stays inside the broker
- the config is user-owned

## Execution Policy

`ote-config.config` controls the local policy.

The important fields are:

- `sandbox.enabled`
- `sandbox.allowed_paths`
- `sandbox.denied_paths`
- `sandbox.allowed_env`
- `runtime.default_shell`
- `runtime.execution_mode`

Execution modes:

- `safe`: allowlisted commands only
- `permission`: ask the user before executing
- `bypass`: skip the prompt but keep policy and path checks

## Path Policy

The broker rejects commands when:

- the working directory is missing
- the working directory is outside `allowed_paths`
- the working directory appears in `denied_paths`
- the command contains blocked syntax
- the command uses destructive verbs

## MCP Policy

The MCP server is local and stdio-based.
It exposes only the tools and resources needed for agent work.

It does not:

- expose raw SKs
- edit the config
- bypass the sandbox policy
- run commands outside the broker

## Operational Advice

- keep `execution_mode` on `safe` by default
- reserve `permission` for interactive sessions
- use `bypass` only when the caller is trusted
- keep `denied_paths` populated for high-risk locations
- avoid broad write access in `allowed_paths`

