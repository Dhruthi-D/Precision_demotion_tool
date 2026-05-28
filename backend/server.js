import express from "express";
import { spawn } from "node:child_process";
import { randomUUID } from "node:crypto";
import { createReadStream, existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { readdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const projectRoot = path.resolve(__dirname, "..");
const buildDir = path.join(projectRoot, "build");
const testDir = path.join(projectRoot, "test");
const jobsRoot = path.join(__dirname, "jobs");

mkdirSync(jobsRoot, { recursive: true });

const app = express();
const port = Number(process.env.PORT || 3001);
const host = process.env.HOST || "127.0.0.1";
const jobs = new Map();

app.use(express.json({ limit: "2mb" }));

app.use((req, res, next) => {
  res.setHeader("Access-Control-Allow-Origin", process.env.FRONTEND_ORIGIN || "http://localhost:5173");
  res.setHeader("Access-Control-Allow-Headers", "Content-Type");
  res.setHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  if (req.method === "OPTIONS") {
    res.sendStatus(204);
    return;
  }
  next();
});

function createJob(kind) {
  const id = randomUUID();
  const dir = path.join(jobsRoot, id);
  mkdirSync(dir, { recursive: true });

  const job = {
    id,
    kind,
    dir,
    status: "queued",
    exitCode: null,
    logs: [],
    clients: new Set(),
    logPath: path.join(dir, "analysis.log"),
    resultPath: path.join(dir, "result.json"),
    astDotPath: path.join(dir, "ast.dot"),
  };

  jobs.set(id, job);
  return job;
}

function pushEvent(job, event, data) {
  const payload = JSON.stringify(data);
  job.logs.push({ event, data });

  if (event === "log" && typeof data?.text === "string") {
    writeFileSync(job.logPath, data.text, { flag: "a" });
  }

  for (const client of job.clients) {
    client.write(`event: ${event}\n`);
    client.write(`data: ${payload}\n\n`);
  }
}

function runProcess(job, command, args, options = {}) {
  job.status = "running";
  pushEvent(job, "status", { status: job.status });
  pushEvent(job, "log", { stream: "system", text: `$ ${command} ${args.join(" ")}\n` });

  const child = spawn(command, args, {
    cwd: options.cwd || projectRoot,
    env: process.env,
    shell: false,
  });

  child.stdout.on("data", (data) => {
    pushEvent(job, "log", { stream: "stdout", text: data.toString() });
  });

  child.stderr.on("data", (data) => {
    pushEvent(job, "log", { stream: "stderr", text: data.toString() });
  });

  child.on("error", (error) => {
    job.status = "error";
    pushEvent(job, "log", { stream: "stderr", text: `${error.message}\n` });
    pushEvent(job, "done", { status: job.status, exitCode: null });
  });

  child.on("close", (code) => {
    job.status = code === 0 ? "completed" : "failed";
    job.exitCode = code;
    pushEvent(job, "done", { status: job.status, exitCode: code });
  });
}

function resolveFromBuild(relativePath) {
  if (!relativePath) return null;
  return path.resolve(buildDir, relativePath);
}

function getJobOr404(req, res) {
  const job = jobs.get(req.params.id);
  if (!job) {
    res.status(404).json({ error: "Job not found" });
    return null;
  }
  return job;
}

app.get("/api/health", (req, res) => {
  res.json({
    ok: true,
    projectRoot,
    buildExists: existsSync(buildDir),
    verifyExists: existsSync(path.join(buildDir, "verify")),
    toolExists: existsSync(path.join(buildDir, "tool")),
  });
});

app.get("/api/kernels", async (req, res, next) => {
  try {
    const files = await readdir(testDir);
    res.json(files.filter((file) => file.endsWith(".cpp")).sort());
  } catch (error) {
    next(error);
  }
});

app.get("/api/kernels/:name", (req, res) => {
  const filePath = path.join(testDir, path.basename(req.params.name));
  if (!filePath.endsWith(".cpp") || !existsSync(filePath)) {
    res.status(404).json({ error: "Kernel not found" });
    return;
  }
  res.type("text/plain").send(readFileSync(filePath, "utf8"));
});

app.post("/api/build", (req, res) => {
  const job = createJob("build");
  runProcess(job, "cmake", ["--build", "build"]);
  res.status(202).json({ jobId: job.id });
});

app.post("/api/run", (req, res) => {
  const { source, fileName = "kernel.cpp", tolerance = "0.01", ast = true } = req.body || {};

  if (typeof source !== "string" || source.trim() === "") {
    res.status(400).json({ error: "source is required" });
    return;
  }

  const numericTolerance = Number(tolerance);
  if (!Number.isFinite(numericTolerance) || numericTolerance < 0) {
    res.status(400).json({ error: "tolerance must be a positive number" });
    return;
  }

  const job = createJob("verify");
  const safeName = path.basename(fileName).replace(/[^a-zA-Z0-9_.-]/g, "_") || "kernel.cpp";
  const inputPath = path.join(job.dir, safeName.endsWith(".cpp") ? safeName : `${safeName}.cpp`);
  writeFileSync(inputPath, source);

  const args = [
    path.relative(buildDir, inputPath),
    String(tolerance),
    `--json=${path.relative(buildDir, job.resultPath)}`,
  ];

  if (ast) {
    args.push(`--ast-dot=${path.relative(buildDir, job.astDotPath)}`);
  }

  runProcess(job, "./verify", args, { cwd: buildDir });
  res.status(202).json({ jobId: job.id });
});

app.get("/api/jobs/:id/events", (req, res) => {
  const job = getJobOr404(req, res);
  if (!job) return;

  res.setHeader("Content-Type", "text/event-stream");
  res.setHeader("Cache-Control", "no-cache");
  res.setHeader("Connection", "keep-alive");
  res.flushHeaders?.();

  job.clients.add(res);

  for (const item of job.logs) {
    res.write(`event: ${item.event}\n`);
    res.write(`data: ${JSON.stringify(item.data)}\n\n`);
  }

  req.on("close", () => {
    job.clients.delete(res);
  });
});

app.get("/api/jobs/:id", (req, res) => {
  const job = getJobOr404(req, res);
  if (!job) return;

  let result = null;
  if (existsSync(job.resultPath)) {
    result = JSON.parse(readFileSync(job.resultPath, "utf8"));
  }

  res.json({
    id: job.id,
    kind: job.kind,
    status: job.status,
    exitCode: job.exitCode,
    result,
    files: {
      input: `/api/jobs/${job.id}/files/input`,
      analysis: `/api/jobs/${job.id}/files/analysis`,
      original: `/api/jobs/${job.id}/files/original`,
      demoted: `/api/jobs/${job.id}/files/demoted`,
      astDot: `/api/jobs/${job.id}/files/ast-dot`,
      astSvg: `/api/jobs/${job.id}/files/ast-svg`,
      csv: `/api/jobs/${job.id}/files/csv`,
    },
  });
});

app.get("/api/jobs/:id/files/:kind", (req, res) => {
  const job = getJobOr404(req, res);
  if (!job) return;

  let filePath = null;
  let contentType = "text/plain";

  const result = existsSync(job.resultPath)
    ? JSON.parse(readFileSync(job.resultPath, "utf8"))
    : null;

  if (req.params.kind === "input") {
    const candidates = readdirSyncSafe(job.dir).filter((file) => file.endsWith(".cpp"));
    filePath = candidates.length ? path.join(job.dir, candidates[0]) : null;
  } else if (req.params.kind === "analysis") {
    filePath = job.logPath;
  } else if (req.params.kind === "original") {
    filePath = resolveFromBuild(result?.paths?.originalSource);
  } else if (req.params.kind === "demoted") {
    filePath = resolveFromBuild(result?.paths?.demotedSource);
  } else if (req.params.kind === "ast-dot") {
    filePath = resolveFromBuild(result?.paths?.astDot) || job.astDotPath;
  } else if (req.params.kind === "csv") {
    filePath = resolveFromBuild(result?.paths?.csv);
  } else if (req.params.kind === "ast-svg") {
    filePath = resolveFromBuild(result?.paths?.astDot) || job.astDotPath;
    contentType = "image/svg+xml";
  }

  if (!filePath || !existsSync(filePath)) {
    res.status(404).json({ error: "File not available yet" });
    return;
  }

  if (req.params.kind === "ast-svg") {
    const dot = spawn("dot", ["-Tsvg", filePath], { cwd: projectRoot, shell: false });
    res.type(contentType);
    dot.stdout.pipe(res);
    dot.stderr.on("data", () => {});
    dot.on("error", () => {
      if (!res.headersSent) res.status(500).end();
    });
    return;
  }

  res.type(contentType);
  createReadStream(filePath).pipe(res);
});

function readdirSyncSafe(dir) {
  try {
    return readdirSync(dir);
  } catch {
    return [];
  }
}

app.use((error, req, res, next) => {
  console.error(error);
  res.status(500).json({ error: error.message || "Internal server error" });
});

const server = app.listen(port, host, () => {
  console.log(`Precision demotion backend listening on http://${host}:${port}`);
});

server.on("error", (error) => {
  if (error.code === "EADDRINUSE") {
    console.error(`Backend port ${host}:${port} is already in use.`);
    console.error("If the dashboard is already open, keep using it; otherwise stop the old backend and retry.");
    process.exit(1);
  }

  throw error;
});
