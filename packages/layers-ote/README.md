# layers-ote

`layers-ote` is the Node.js bridge for OTE-backed `.env` proxy workflows.

## Install

```bash
npm install layers-ote
```

## Use

```js
const { injectIntoProcessEnv } = require("layers-ote");

injectIntoProcessEnv();
```

If `ote` is not on `PATH`, pass `binaryPath` or set `OTE_BINARY`.

## TypeScript

```ts
import { injectIntoProcessEnv } from "layers-ote";

injectIntoProcessEnv();
```

## Runtime Contract

The package reads the local proxy `.env`, extracts `OTE_PROFILE`, and asks OTE to materialize the trusted environment into the current process.
