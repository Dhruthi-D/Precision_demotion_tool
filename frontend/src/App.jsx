import React, { useEffect, useMemo, useRef, useState } from "react";
import { createRoot } from "react-dom/client";
import "./styles.css";

const apiBase = import.meta.env.VITE_API_BASE || "http://localhost:3001";

const initialSource = `#include <iostream>

int main() {
    float a = 1.25f;
    float b = 2.5f;
    float c = (a * b) + 0.125f;

    std::cout << c << std::endl;
    return 0;
}
`;

function formatNumber(value, suffix = "") {
  if (value === undefined || value === null || Number.isNaN(Number(value))) {
    return "Pending";
  }
  return `${Number(value).toFixed(4)}${suffix}`;
}

function parseCsvLine(line) {
  const cells = [];
  let cell = "";
  let quoted = false;

  for (let index = 0; index < line.length; index += 1) {
    const char = line[index];
    const next = line[index + 1];

    if (char === '"' && quoted && next === '"') {
      cell += '"';
      index += 1;
    } else if (char === '"') {
      quoted = !quoted;
    } else if (char === "," && !quoted) {
      cells.push(cell);
      cell = "";
    } else {
      cell += char;
    }
  }

  cells.push(cell);
  return cells;
}

function parseCsv(text) {
  return text
    .trim()
    .split(/\r?\n/)
    .filter(Boolean)
    .map(parseCsvLine);
}

function displayCsvCell(value, cellIndex) {
  if (cellIndex !== 0) return value;

  return value.split(/[\\/]/).pop() || value;
}

function getAnalysisCardTitle(line) {
  const trimmed = line.trim();

  if (/^\[\d+\]\s*AST DATA FLOW ANALYSIS$/i.test(trimmed)) {
    return "AST Data Flow Analysis";
  }

  if (/^\[\d+\]\s*BACKWARD PRECISION PROPAGATION$/i.test(trimmed)) {
    return "Backward Precision Propagation";
  }

  if (/^\[\d+\]\s*AST REWRITING$/i.test(trimmed)) {
    return "AST Rewriting";
  }

  if (/^Verification Report$/i.test(trimmed)) {
    return "Verification Report";
  }

  return null;
}

function parseAnalysisLog(text) {
  const lines = text.split(/\r?\n/);
  const sectionsByTitle = new Map();
  let current = null;

  function pushCurrent() {
    if (!current) return;
    const body = current.lines.join("\n").trim();
    if (!body) return;

    const section = sectionsByTitle.get(current.title) || { title: current.title, blocks: [] };
    section.blocks.push(body);
    sectionsByTitle.set(current.title, section);
  }

  for (const line of lines) {
    if (/^\s*=+\s*$/.test(line)) continue;

    const title = getAnalysisCardTitle(line);
    if (title) {
      pushCurrent();
      current = { title, lines: [] };
      continue;
    }

    const isReturnFlow = /^\[RETURN FLOW\]$/i.test(line.trim());
    const isOtherHeading = /^\[[^\]]+\]/.test(line.trim()) || /^(PRECISION DEMOTION FRAMEWORK|ANALYSIS COMPLETE)$/i.test(line.trim());
    if (isOtherHeading && !(isReturnFlow && current?.title === "AST Data Flow Analysis")) {
      pushCurrent();
      current = null;
      continue;
    }

    if (!current) continue;
    current.lines.push(line);
  }

  pushCurrent();
  return ["AST Data Flow Analysis", "Backward Precision Propagation", "AST Rewriting", "Verification Report"]
    .map((title) => sectionsByTitle.get(title))
    .filter(Boolean);
}

function parseSectionRows(body) {
  return body
    .split(/\r?\n/)
    .map((line) => line.match(/^\s*([^:]{2,}?)\s*:\s*(.*)\s*$/))
    .filter(Boolean)
    .map((match) => [match[1].trim(), match[2].trim()]);
}

function getAstDataFlowRows(block) {
  const rows = parseSectionRows(block);
  const values = new Map(rows.map(([label, value]) => [label.toLowerCase(), value]));
  const unmatchedLines = block
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean)
    .filter((line) => !/^\s*[^:]{2,}?\s*:\s*.*$/.test(line));

  const orderedRows = [
    ["Variable", values.get("variable") || "-"],
    ["Flow", values.get("flow") || "-"],
    ["Depends", values.get("depends") || "-"],
    ["Local Error Estimate", values.get("local error estimate") || "-"],
  ];

  if (unmatchedLines.length > 0) {
    orderedRows.push(["Details", unmatchedLines.join(" ")]);
  }

  return orderedRows;
}

function AnalysisCards({ text }) {
  const sections = useMemo(() => parseAnalysisLog(text), [text]);

  if (!text.trim()) {
    return <div className="analysis-empty">Analysis output will appear after a run completes.</div>;
  }

  return (
    <div className="analysis-cards">
      {sections.map((section, index) => {
        const sectionClass = section.title.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");

        return (
          <article className={`analysis-card analysis-card-${sectionClass}`} key={`${section.title}-${index}`}>
            <header>
              <h2>{section.title}</h2>
            </header>
            <div className="analysis-blocks">
              {section.blocks.map((block, blockIndex) => {
                const isAstDataFlow = section.title === "AST Data Flow Analysis";
                const rows = isAstDataFlow ? getAstDataFlowRows(block) : parseSectionRows(block);
                const hasOnlyRows = rows.length > 0 && (
                  isAstDataFlow || rows.length === block.split(/\r?\n/).filter(Boolean).length
                );

                return (
                  <div className="analysis-block" key={`${section.title}-${blockIndex}`}>
                    {hasOnlyRows ? (
                      <dl className="analysis-kv">
                        {rows.map(([label, value]) => (
                          <React.Fragment key={`${label}-${value}`}>
                            <dt>{label}</dt>
                            <dd className={value === "PASS" ? "pass-text" : value === "FAIL" ? "fail-text" : ""}>
                              {value || "-"}
                            </dd>
                          </React.Fragment>
                        ))}
                      </dl>
                    ) : (
                      <pre>{block}</pre>
                    )}
                  </div>
                );
              })}
            </div>
          </article>
        );
      })}
    </div>
  );
}

function App() {
  const [kernels, setKernels] = useState([]);
  const [selectedKernel, setSelectedKernel] = useState("");
  const [fileName, setFileName] = useState("kernel.cpp");
  const [source, setSource] = useState(initialSource);
  const [tolerance, setTolerance] = useState("0.01");
  const [logs, setLogs] = useState("");
  const [jobId, setJobId] = useState("");
  const [job, setJob] = useState(null);
  const [running, setRunning] = useState(false);
  const [activeFile, setActiveFile] = useState("demoted");
  const [fileContent, setFileContent] = useState("");
  const [fileError, setFileError] = useState("");
  const [astZoom, setAstZoom] = useState(1);
  const logRef = useRef(null);

  const result = job?.result;
  const csvRows = useMemo(() => (
    activeFile === "csv" && !fileError && fileContent ? parseCsv(fileContent) : []
  ), [activeFile, fileContent, fileError]);
  const analysisText = activeFile === "analysis" ? fileContent || logs : "";

  const metrics = useMemo(() => [
    ["Verification", result?.verification || "Pending"],
    ["Accuracy Loss", formatNumber(result?.accuracyLoss)],
    ["Tolerance", formatNumber(result?.tolerance)],
    ["Speedup", formatNumber(result?.speedup, "x")],
    ["Performance Gain", formatNumber(result?.performanceGainPercent, "%")],
    ["Original Runtime", formatNumber(result?.originalRuntimeMs, " ms")],
    ["Demoted Runtime", formatNumber(result?.demotedRuntimeMs, " ms")],
    ["Original Compile", formatNumber(result?.originalCompileTimeMs, " ms")],
    ["Demoted Compile", formatNumber(result?.demotedCompileTimeMs, " ms")],
  ], [result]);

  useEffect(() => {
    fetch(`${apiBase}/api/kernels`)
      .then((response) => response.json())
      .then(setKernels)
      .catch(() => setKernels([]));
  }, []);

  useEffect(() => {
    if (logRef.current) {
      logRef.current.scrollTop = logRef.current.scrollHeight;
    }
  }, [logs]);

  useEffect(() => {
    if (!jobId) return;

    setFileContent("");
    setFileError("");

    if (activeFile === "ast") return;

    fetch(`${apiBase}/api/jobs/${jobId}/files/${activeFile}`)
      .then(async (response) => {
        if (!response.ok) throw new Error("File is not available yet.");
        return response.text();
      })
      .then(setFileContent)
      .catch((error) => setFileError(error.message));
  }, [activeFile, jobId, job?.status]);

  async function loadKernel(name) {
    setSelectedKernel(name);
    if (!name) return;

    setFileName(name);

    const response = await fetch(`${apiBase}/api/kernels/${name}`);
    const text = await response.text();
    setSource(text);
  }

  async function startBuild() {
    setRunning(true);
    setLogs("");
    setJob(null);

    const response = await fetch(`${apiBase}/api/build`, { method: "POST" });
    const data = await response.json();
    attachEvents(data.jobId);
  }

  async function startRun() {
    setRunning(true);
    setLogs("");
    setJob(null);
    setActiveFile("analysis");

    const response = await fetch(`${apiBase}/api/run`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        source,
        fileName,
        tolerance,
        ast: true,
      }),
    });

    const data = await response.json();
    if (!response.ok) {
      setRunning(false);
      setLogs(data.error || "Run request failed.");
      return;
    }

    attachEvents(data.jobId);
  }

  function attachEvents(nextJobId) {
    setJobId(nextJobId);
    const stream = new EventSource(`${apiBase}/api/jobs/${nextJobId}/events`);

    stream.addEventListener("log", (event) => {
      const data = JSON.parse(event.data);
      setLogs((current) => `${current}${data.text}`);
    });

    stream.addEventListener("done", async () => {
      stream.close();
      setRunning(false);
      const response = await fetch(`${apiBase}/api/jobs/${nextJobId}`);
      setJob(await response.json());
    });
  }

  return (
    <main className="app-shell">
      <header className="topbar">
        <div>
          <h1>Precision Demotion Tool</h1>
          <p>Run the C++ verifier from the browser and inspect every generated artifact.</p>
        </div>
        <div className="actions">
          <button type="button" onClick={startBuild} disabled={running}>Build</button>
          <button type="button" className="primary" onClick={startRun} disabled={running}>Run Verify</button>
        </div>
      </header>

      <section className="workspace">
        <aside className="control-panel">
          <label>
            Example kernel
            <select value={selectedKernel} onChange={(event) => loadKernel(event.target.value)}>
              <option value="">Custom source</option>
              {kernels.map((kernel) => (
                <option key={kernel} value={kernel}>{kernel}</option>
              ))}
            </select>
          </label>

          <label>
            File name
            <input value={fileName} onChange={(event) => setFileName(event.target.value)} />
          </label>

          <label>
            Tolerance
            <input value={tolerance} onChange={(event) => setTolerance(event.target.value)} inputMode="decimal" />
          </label>

          <div className={`status ${result?.passed ? "pass" : result ? "fail" : ""}`}>
            <span>{running ? "Running" : result?.verification || "Ready"}</span>
          </div>
        </aside>

        <section className="editor-pane">
          <div className="pane-title">Kernel Source</div>
          <textarea spellCheck="false" value={source} onChange={(event) => setSource(event.target.value)} />
        </section>
      </section>

      <section className="results-grid">
        <section className="metrics-panel">
          <div className="pane-title">Metrics</div>
          <div className="metrics-grid">
            {metrics.map(([label, value]) => (
              <div className="metric" key={label}>
                <span>{label}</span>
                <strong>{value}</strong>
              </div>
            ))}
          </div>
        </section>

        <section className="terminal-panel">
          <div className="pane-title">Terminal Output</div>
          <pre ref={logRef}>{logs || "No run started yet."}</pre>
        </section>
      </section>

      <section className="artifact-panel">
        <div className="artifact-tabs">
          {["analysis", "input", "demoted", "ast", "csv"].map((name) => (
            <button
              key={name}
              type="button"
              className={activeFile === name ? "active" : ""}
              onClick={() => setActiveFile(name)}
              disabled={!jobId}
            >
              {name}
            </button>
          ))}
        </div>

        {activeFile === "ast" && jobId ? (
          <div className="ast-viewer">
            <div className="ast-toolbar">
              <button type="button" onClick={() => setAstZoom((zoom) => Math.max(0.25, zoom - 0.25))}>
                Zoom -
              </button>
              <span>{Math.round(astZoom * 100)}%</span>
              <button type="button" onClick={() => setAstZoom((zoom) => Math.min(2.5, zoom + 0.25))}>
                Zoom +
              </button>
              <button type="button" onClick={() => setAstZoom(1)}>
                Fit
              </button>
            </div>
            <div className="ast-frame">
              <img
                src={`${apiBase}/api/jobs/${jobId}/files/ast-svg?${job?.status || ""}`}
                alt="Expression AST"
                style={{ height: `${astZoom * 100}%` }}
              />
            </div>
          </div>
        ) : activeFile === "analysis" ? (
          <AnalysisCards text={fileError && !analysisText ? fileError : analysisText} />
        ) : activeFile === "csv" && csvRows.length ? (
          <div className="csv-table-wrap">
            <table className="csv-table">
              <thead>
                <tr>
                  {csvRows[0].map((heading, index) => (
                    <th key={`${heading}-${index}`}>{heading}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {csvRows.slice(1).map((row, rowIndex) => (
                  <tr key={`row-${rowIndex}`}>
                    {csvRows[0].map((_, cellIndex) => (
                      <td key={`cell-${cellIndex}`}>{displayCsvCell(row[cellIndex] || "", cellIndex)}</td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        ) : (
          <pre className="artifact-code">{fileError || fileContent || "Artifact will appear after a run completes."}</pre>
        )}
      </section>
    </main>
  );
}

createRoot(document.getElementById("root")).render(<App />);
