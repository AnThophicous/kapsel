const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");

function readProxyEnv(cwd = process.cwd()) {
  const envPath = path.join(cwd, ".env");
  if (!fs.existsSync(envPath)) {
    return { profile: "prod", path: envPath };
  }

  const text = fs.readFileSync(envPath, "utf8");
  const lines = text.split(/\r?\n/);
  for (const line of lines) {
    const trimmed = line.trim();
    if (!trimmed || trimmed.startsWith("#")) {
      continue;
    }
    const index = trimmed.indexOf("=");
    if (index === -1) {
      continue;
    }
    const key = trimmed.slice(0, index).trim();
    const value = trimmed.slice(index + 1).trim();
    if (key === "OTE_PROFILE") {
      return { profile: value || "prod", path: envPath };
    }
  }

  return { profile: "prod", path: envPath };
}

function resolveBinaryPath(options = {}) {
  if (options.binaryPath) {
    return options.binaryPath;
  }
  if (process.env.OTE_BINARY) {
    return process.env.OTE_BINARY;
  }
  return process.platform === "win32" ? "ote.exe" : "ote";
}

function materialize(profile, cwd = process.cwd(), options = {}) {
  const exe = resolveBinaryPath(options);
  const result = spawnSync(exe, ["bridge", "materialize", profile], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"]
  });

  if (result.status !== 0) {
    const error = result.stderr || result.stdout || "layers materialize failed";
    throw new Error(error.trim());
  }

  const payload = JSON.parse(result.stdout);
  return payload.env || {};
}

function injectIntoProcessEnv(options = {}) {
  const cwd = options.cwd || process.cwd();
  const proxy = readProxyEnv(cwd);
  const profile = options.profile || proxy.profile;
  const env = materialize(profile, cwd, options);

  for (const [key, value] of Object.entries(env)) {
    process.env[key] = value;
  }

  if (options.includeProfile !== false) {
    process.env.OTE_PROFILE = profile;
  }

  return env;
}

module.exports = {
  readProxyEnv,
  resolveBinaryPath,
  materialize,
  injectIntoProcessEnv
};
