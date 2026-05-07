# OTE Agent Integration

This document is for agent authors and local tool integrators.

## Principle

The agent can ask OTE to do work.
The agent cannot see raw SKs.

OTE exists to make that boundary boring and reliable.

## Integration Checklist

1. start OTE from the project root
2. run `ote --setup` once
3. verify `ote --doctor`
4. wire `ote mcp install <target>` or `ote mcp install --config <path>` into the agent runtime
5. verify with `ote mcp doctor`
6. keep the config file user-owned
7. let the broker decide whether a command can run

## Suggested Agent Behavior

- ask for `secret.list` when you need to discover available profiles
- ask for `secret.describe` when you need metadata and allowed keys
- ask for `exec.plan` before `exec.run`
- ask for `mcp config` when you need the raw client snippet
- ask for `mcp install` when you need the installer to write the client file
- ask for `bridge env` when a runtime needs the managed environment
- ask for `bridge run` when OTE should launch the child process with the bridge applied
- prefer `safe` by default
- use `permission` only when a human is present
- never request raw secret values

## Good Prompts

Use short, structured requests.

Examples:

- "List available secret profiles for deployment."
- "Describe the redacted keys for `api-prod`."
- "Build an execution plan for `git status`."
- "Run this read-only command through OTE."

## Bad Prompts

Do not ask OTE to:

- print secret values
- dump decrypted vault contents
- edit `ote-config.config`
- execute outside the configured paths
- bypass path checks

## Client Flow

Recommended installer flow for agent authors:

1. call `ote mcp config` to preview the JSON
2. call `ote mcp install <target>` to merge into the client config
3. call `ote mcp doctor` to verify local wiring
4. restart or reload the client

If the client is not one of the built-in targets, pass `--config <path>` and let OTE merge only `mcpServers.ote`.

## Recommended Policy

For agent-friendly but safer usage:

- `safe` for default automation
- `permission` for human-approved actions
- `bypass` only for trusted local automation

Keep `allowed_paths` narrow.
Keep `denied_paths` explicit.
Keep `allowed_env` short.

## Minimal Example

```json
{
  "version": 1,
  "sandbox": {
    "enabled": true,
    "allowed_paths": ["C:\\OTE"],
    "denied_paths": ["C:\\", "C:\\Windows\\System32"],
    "allowed_env": ["PATH", "SYSTEMROOT", "WINDIR", "HOME", "USERPROFILE", "TMP", "TEMP"]
  },
  "runtime": {
    "default_shell": "cmd",
    "execution_mode": "safe",
    "cache_dir": "C:\\OTE\\.ote\\cache"
  }
}
```

## Operational Rule

If a request would require the agent to see the secret, the answer is no.
The agent should instead ask OTE to execute the intended action with the secret context applied locally.
