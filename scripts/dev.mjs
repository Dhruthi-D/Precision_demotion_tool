import http from "node:http";
import { spawn } from "node:child_process";

const backendUrl = new URL(process.env.VITE_API_BASE || "http://127.0.0.1:3001");
const children = [];

function probeBackend() {
  return new Promise((resolve) => {
    const request = http.get(
      {
        hostname: backendUrl.hostname,
        port: backendUrl.port || 80,
        path: "/api/health",
        timeout: 800,
      },
      (response) => {
        response.resume();
        resolve(response.statusCode === 200 ? "healthy" : "occupied");
      },
    );

    request.on("timeout", () => {
      request.destroy();
      resolve("occupied");
    });
    request.on("error", (error) => {
      resolve(error.code === "ECONNREFUSED" ? "free" : "occupied");
    });
  });
}

function run(label, command, args, options = {}) {
  const child = spawn(command, args, {
    cwd: options.cwd || process.cwd(),
    env: { ...process.env, ...options.env },
    shell: process.platform === "win32",
    stdio: "inherit",
  });

  child.once("exit", (code, signal) => {
    if (signal) return;
    if (code !== 0) {
      console.error(`${label} exited with code ${code}.`);
    }
    stopAll(child);
    process.exitCode = code ?? 1;
  });

  children.push(child);
  return child;
}

function stopAll(except) {
  for (const child of children) {
    if (child !== except && !child.killed) {
      child.kill("SIGTERM");
    }
  }
}

process.on("SIGINT", () => {
  stopAll();
  process.exit(130);
});

process.on("SIGTERM", () => {
  stopAll();
  process.exit(143);
});

const backendStatus = await probeBackend();

if (backendStatus === "healthy") {
  console.log(`Backend already running at ${backendUrl.origin}; reusing it.`);
} else if (backendStatus === "occupied") {
  console.error(`Port ${backendUrl.origin} is in use, but it does not look like this backend.`);
  console.error("Stop that process or set VITE_API_BASE/PORT to matching free ports, then retry.");
  process.exit(1);
} else {
  run("backend", "npm", ["run", "dev", "--prefix", "backend"]);
}

run("frontend", "npm", ["run", "dev", "--prefix", "frontend"]);
