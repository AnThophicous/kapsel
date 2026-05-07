# OTE Publishing

This document describes the final publish flow for OTE 1.0.2.

## Goal

Ship a public release with:

- source code
- docs
- package artifacts
- SHA metadata

## Recommended Flow

1. create a release branch
2. validate the build locally
3. tag the release
4. push the tag
5. let GitHub Actions build the matrix
6. upload the package artifacts
7. attach the checksums to the release notes

## Local Build

```bash
cmake -S . -B build
cmake --build build
cmake --build build --target package
```

## Artifact Expectations

The release workflow should produce per-platform packages for:

- Windows
- Linux
- macOS

## SHA

For each uploaded package, publish a SHA256 checksum alongside the artifact name and version.

## Final Rule

Raw SKs never go into the repo, the release artifacts, or the MCP surface.
