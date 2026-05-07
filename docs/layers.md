# OTE Layers

Layers is the bridge layer that turns a normal `.env` workflow into a local OTE-backed proxy.

It is designed for trusted application runtimes, not for the agent surface.

## What It Does

- migrates a project `.env` into OTE
- rewrites `.env` into a proxy file
- stores the protected values in the OTE vault
- exposes a language bridge for Node.js, TypeScript, and Python

## Migration

Run:

```bash
ote --migration
```

This will:

- read the local `.env`
- infer `OTE_PROFILE` or default to `prod`
- store the non-OTE variables in a protected OTE secret
- rewrite `.env` to:

```env
# Managed by OTE
OTE_PROFILE=prod
```

## Bridge Contract

The bridge packages read the proxy `.env`, extract the profile, and ask OTE to materialize the values into the trusted runtime.

That keeps application code simple:

- `process.env` still works in Node.js
- `os.environ` still works in Python
- the real source of truth stays in OTE

## Node.js

Install the package:

```bash
npm install layers-ote
```

Usage:

```js
const { injectIntoProcessEnv } = require("layers-ote");

injectIntoProcessEnv();
```

If `ote` is not on `PATH`, pass `binaryPath` or set `OTE_BINARY`.

## TypeScript

TypeScript uses the same package and the bundled `.d.ts` file.

```ts
import { injectIntoProcessEnv } from "layers-ote";

injectIntoProcessEnv();
```

## Python

The Python bridge exposes the same concept:

```py
from layers_ote import inject_into_process_env

inject_into_process_env()
```

If `ote` is not on `PATH`, pass `binary_path` or set `OTE_BINARY`.

## Security Boundary

The agent should never be given direct access to the secret payload.
The bridge is only for trusted local application runtime.
