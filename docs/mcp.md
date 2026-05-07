# OTE MCP

OTE ships a local stdio MCP server for agent tooling.

The goal is simple:

- give the agent structured access to OTE
- keep raw SKs hidden forever
- let the broker enforce local policy

## Transport

Use stdio.

Start the server with:

```bash
ote mcp serve
```

The server speaks framed JSON-RPC over standard input and output.

## Manifest

Generate the manifest with:

```bash
ote mcp manifest
```

The manifest tells the agent:

- the current platform
- the current architecture
- the active protector
- the available tool surface

## Tools

The MCP server exposes these tools:

- `secret.list`
- `secret.describe`
- `secret.add`
- `exec.plan`
- `exec.run`
- `status`
- `paths`
- `config.show`

### Secret tools

- `secret.list` returns redacted projections
- `secret.describe` returns one redacted projection
- `secret.add` stores a protected secret locally

### Execution tools

- `exec.plan` builds a policy-checked plan
- `exec.run` runs through the broker

In `permission` mode, `exec.run` returns an error in MCP mode because approval is interactive by design.

### Inspection tools

- `status` returns runtime status
- `paths` returns the local OTE layout
- `config.show` returns the current config snapshot

## Resources

The server also exposes read-only resources:

- `ote://status`
- `ote://paths`
- `ote://config`
- `ote://manifest`
- `ote://root`

## Agent Config

Most MCP clients use a JSON config that looks like this:

```json
{
  "mcpServers": {
    "ote": {
      "command": "ote",
      "args": ["mcp", "serve"],
      "cwd": "C:\\OTE"
    }
  }
}
```

On Unix-like systems, the command is usually the local binary path:

```json
{
  "mcpServers": {
    "ote": {
      "command": "./ote",
      "args": ["mcp", "serve"],
      "cwd": "/path/to/OTE"
    }
  }
}
```

## Recommended Flow For Agents

1. read `ote mcp manifest`
2. inspect `status` and `paths`
3. use `secret.list` and `secret.describe` to choose the right profile
4. call `exec.plan` before `exec.run`
5. keep `safe` as the default mode
6. request `permission` only when the user is present

## What The Agent Never Gets

- raw SKs
- decrypted payloads
- direct config writes
- bypass of path policy

