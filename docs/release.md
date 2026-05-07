# OTE Release

OTE 1.0.2 is meant to ship as a cross-platform package with documentation, MCP support, Layers, install helpers, and update tooling.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Package

CPack produces:

- `ZIP`
- `TGZ`

## Matrix

The release workflow targets:

- Windows
- Linux
- macOS

## GitHub Flow

1. initialize a git repository
2. create the GitHub remote
3. commit the release-ready code
4. push the branch
5. create a draft release or PR

## Local Checks

Before tagging:

- `ote --doctor`
- `ote --validate`
- `ote --status`
- `ote --putpath`
- `ote update`
- `ote mcp manifest`
- `ote mcp config`
- `ote mcp install <target>`
- `ote mcp doctor`
- `ote mcp serve`

## Security Note

The release artifacts should include code, docs, and build outputs.
They should not include raw secret material.
