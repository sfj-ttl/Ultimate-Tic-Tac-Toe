const backendOrigin = "http://localhost:8080";
const servedByBackend = ["localhost:8080", "127.0.0.1:8080"].includes(location.host);
const apiBase = servedByBackend ? "" : backendOrigin;

const menuScreen = document.querySelector("#menuScreen");
const gameScreen = document.querySelector("#gameScreen");
const menuStatusEl = document.querySelector("#menuStatus");
const startBtn = document.querySelector("#startGame");
const backBtn = document.querySelector("#backToMenu");
const modeCards = Array.from(document.querySelectorAll("[data-menu-mode]"));
const levelCards = Array.from(document.querySelectorAll("[data-ai-level]"));
const aiOptions = document.querySelector("#aiOptions");

const board = document.querySelector("#board");
const macro = document.querySelector("#macro");
const statusEl = document.querySelector("#status");
const turnEl = document.querySelector("#turn");
const forcedEl = document.querySelector("#forced");
const lastMoveEl = document.querySelector("#lastMove");
const modeInfoEl = document.querySelector("#modeInfo");
const levelInfoEl = document.querySelector("#levelInfo");
const newBtn = document.querySelector("#newGame");

const winPatterns = [
  [[0, 0], [0, 1], [0, 2]],
  [[1, 0], [1, 1], [1, 2]],
  [[2, 0], [2, 1], [2, 2]],
  [[0, 0], [1, 0], [2, 0]],
  [[0, 1], [1, 1], [2, 1]],
  [[0, 2], [1, 2], [2, 2]],
  [[0, 0], [1, 1], [2, 2]],
  [[0, 2], [1, 1], [2, 0]],
];

let state;
let selectedMode = "pvp";
let selectedAiLevel = 2;
let currentMode = "pvp";
let currentAiLevel = 2;
let aiThinking = false;
const aiPlayer = "O";
const lockedBoardLines = new Map();
let lockedMacroLine = null;

function cls(value) {
  return value === "X" ? "x" : value === "O" ? "o" : value === "D" ? "draw" : "";
}

function aiLevelName(level) {
  if (level === 1) return "简单";
  if (level === 3) return "困难";
  return "普通";
}

function isAiMode() {
  return currentMode === "pve";
}

function isAiTurn() {
  return isAiMode() && state && !state.gameOver && state.currentPlayer === aiPlayer;
}

function legal(row, col) {
  return state && !state.gameOver && state.legalMoves.some((move) => move.row === row && move.col === col);
}

async function req(path, opt = {}) {
  const res = await fetch(apiBase + path, {
    headers: { "Content-Type": "application/json" },
    ...opt,
  });
  const data = await res.json();
  if (!res.ok || data.ok === false) throw new Error(data.message || "请求失败");
  return data;
}

function showMenu(message = "选择对局模式") {
  aiThinking = false;
  menuStatusEl.textContent = message;
  menuScreen.classList.remove("is-hidden");
  gameScreen.classList.add("is-hidden");
}

function showGame() {
  menuScreen.classList.add("is-hidden");
  gameScreen.classList.remove("is-hidden");
  modeInfoEl.textContent = currentMode === "pve" ? "人机对战" : "双人对战";
  levelInfoEl.textContent = currentMode === "pve" ? `AI：${aiLevelName(currentAiLevel)}` : "";
}

function updateMenuSelection() {
  for (const card of modeCards) {
    card.classList.toggle("selected", card.dataset.menuMode === selectedMode);
  }
  for (const card of levelCards) {
    card.classList.toggle("selected", Number(card.dataset.aiLevel) === selectedAiLevel);
  }
  aiOptions.classList.toggle("is-hidden", selectedMode !== "pve");
  menuStatusEl.textContent = selectedMode === "pve" ? "选择 AI 难度" : "选择对局模式";
}

function forcedText() {
  if (state.forcedBigRow < 0) return "任意可用大格";
  return `第 ${state.forcedBigRow + 1} 行，第 ${state.forcedBigCol + 1} 列`;
}

function stateWithLastMove(nextState, previousState = state) {
  if (Number.isInteger(nextState.lastMoveRow) && Number.isInteger(nextState.lastMoveCol)) {
    return nextState;
  }

  const inferred = { ...nextState, lastMoveRow: -1, lastMoveCol: -1 };
  if (!previousState) return inferred;

  for (let row = 0; row < 9; row++) {
    for (let col = 0; col < 9; col++) {
      if (previousState.board[row][col] === "." && nextState.board[row][col] !== ".") {
        inferred.lastMoveRow = row;
        inferred.lastMoveCol = col;
        return inferred;
      }
    }
  }
  return inferred;
}

function lastMoveText() {
  if (!state || !Number.isInteger(state.lastMoveRow) || !Number.isInteger(state.lastMoveCol)) return "-";
  if (state.lastMoveRow < 0 || state.lastMoveCol < 0) return "-";
  return `第 ${state.lastMoveRow + 1} 行，第 ${state.lastMoveCol + 1} 列`;
}

function winnerInGrid(grid) {
  for (const pattern of winPatterns) {
    const values = pattern.map(([row, col]) => grid[row][col]);
    const owner = values[0];
    if ((owner === "X" || owner === "O") && values.every((value) => value === owner)) {
      return {
        owner,
        cells: pattern.map(([row, col]) => ({ row, col })),
      };
    }
  }
  return null;
}

function smallGrid(bigRow, bigCol) {
  return Array.from({ length: 3 }, (_, row) =>
    Array.from({ length: 3 }, (_, col) => state.board[bigRow * 3 + row][bigCol * 3 + col])
  );
}

function detectedBoardWinLines() {
  const lines = [];

  for (let bigRow = 0; bigRow < 3; bigRow++) {
    for (let bigCol = 0; bigCol < 3; bigCol++) {
      const line = winnerInGrid(smallGrid(bigRow, bigCol));
      if (!line) continue;
      lines.push({
        id: `small-${bigRow}-${bigCol}`,
        owner: line.owner,
        cells: line.cells.map(({ row, col }) => ({
          row: bigRow * 3 + row,
          col: bigCol * 3 + col,
        })),
      });
    }
  }

  return lines;
}

function detectedMacroWinLine() {
  const line = winnerInGrid(state.smallBoards);
  if (!line) return null;
  return {
    id: "macro",
    owner: line.owner,
    cells: line.cells,
  };
}

function syncLockedWinLines() {
  for (const line of detectedBoardWinLines()) {
    if (!lockedBoardLines.has(line.id)) lockedBoardLines.set(line.id, line);
  }

  if (!lockedMacroLine) lockedMacroLine = detectedMacroWinLine();
}

function resetLockedWinLines() {
  lockedBoardLines.clear();
  lockedMacroLine = null;
}

function cellKey(row, col) {
  return `${row},${col}`;
}

function winCellKeys(lines) {
  return new Set(lines.flatMap((line) => line.cells.map(({ row, col }) => cellKey(row, col))));
}

function addWinLine(container, line, size) {
  const [start, , end] = line.cells;
  const cellSize = 100 / size;
  const x1 = (start.col + 0.5) * cellSize;
  const y1 = (start.row + 0.5) * cellSize;
  const x2 = (end.col + 0.5) * cellSize;
  const y2 = (end.row + 0.5) * cellSize;
  const dx = x2 - x1;
  const dy = y2 - y1;
  const length = Math.hypot(dx, dy);
  const extension = cellSize * 0.42;
  const unitX = dx / length;
  const unitY = dy / length;
  const lineEl = document.createElement("div");

  lineEl.className = `win-line ${cls(line.owner)}`;
  lineEl.style.left = `${x1 - unitX * extension}%`;
  lineEl.style.top = `${y1 - unitY * extension}%`;
  lineEl.style.width = `${length + extension * 2}%`;
  lineEl.style.transform = `translateY(-50%) rotate(${Math.atan2(dy, dx)}rad)`;
  container.appendChild(lineEl);
}

function renderBoardWinLines(lines) {
  for (const line of lines) addWinLine(board, line, 9);
}

function renderMacroWinLine(line) {
  if (line) addWinLine(macro, line, 3);
}

function render() {
  if (!state) return;

  syncLockedWinLines();

  const smallLines = Array.from(lockedBoardLines.values());
  const smallWinCells = winCellKeys(smallLines);
  const bigLine = lockedMacroLine;
  const bigWinCells = bigLine ? winCellKeys([bigLine]) : new Set();

  statusEl.textContent = aiThinking ? "AI 正在思考..." : state.message;
  turnEl.textContent = state.gameOver ? "-" : `${state.currentPlayer}${isAiTurn() ? " (AI)" : ""}`;
  forcedEl.textContent = forcedText();
  lastMoveEl.textContent = lastMoveText();

  board.innerHTML = "";
  for (let row = 0; row < 9; row++) {
    for (let col = 0; col < 9; col++) {
      const value = state.board[row][col];
      const canPlay = legal(row, col) && !aiThinking && !isAiTurn();
      const big = Math.floor(row / 3) * 3 + Math.floor(col / 3);
      const cell = document.createElement("button");
      cell.type = "button";
      cell.textContent = value === "." ? "" : value;
      cell.dataset.r = row;
      cell.dataset.c = col;
      cell.className = `cell ${cls(value)} ${canPlay ? "legal" : "blocked"}`;
      if (row === state.lastMoveRow && col === state.lastMoveCol) cell.classList.add("last-move");
      if (smallWinCells.has(cellKey(row, col))) cell.classList.add("win-cell");
      if (canPlay && state.forcedBigRow >= 0 && big === state.forcedBigRow * 3 + state.forcedBigCol) {
        cell.classList.add("forced");
      }
      cell.disabled = !canPlay;
      cell.onclick = () => move(row, col);
      board.appendChild(cell);
    }
  }
  renderBoardWinLines(smallLines);

  macro.innerHTML = "";
  for (let row = 0; row < 3; row++) {
    for (let col = 0; col < 3; col++) {
      const value = state.smallBoards[row][col];
      const cell = document.createElement("div");
      cell.className = `mcell ${cls(value)}`;
      cell.textContent = value === "." ? "" : value;
      if (bigWinCells.has(cellKey(row, col))) cell.classList.add("win-cell");
      if (!state.gameOver && state.forcedBigRow === row && state.forcedBigCol === col) {
        cell.classList.add("forced");
      }
      macro.appendChild(cell);
    }
  }
  renderMacroWinLine(bigLine);
}

async function startGame() {
  currentMode = selectedMode;
  currentAiLevel = selectedAiLevel;
  showGame();
  await newGame();
}

async function move(row, col) {
  if (aiThinking || isAiTurn()) return;

  try {
    state = stateWithLastMove(await req("/api/move", {
      method: "POST",
      body: JSON.stringify({ row, col }),
    }));
    render();
    await maybeAiMove();
  } catch (error) {
    statusEl.textContent = error.message;
  }
}

async function maybeAiMove() {
  if (!isAiTurn() || aiThinking) return;

  aiThinking = true;
  render();
  await new Promise((resolve) => setTimeout(resolve, 250));

  try {
    state = stateWithLastMove(await req("/api/ai-move", {
      method: "POST",
      body: JSON.stringify({ level: currentAiLevel }),
    }));
  } catch (error) {
    statusEl.textContent = error.message;
  } finally {
    aiThinking = false;
    render();
  }
}

async function newGame() {
  try {
    aiThinking = false;
    resetLockedWinLines();
    state = stateWithLastMove(await req("/api/new", { method: "POST" }), null);
    render();
    await maybeAiMove();
  } catch (error) {
    statusEl.textContent = "无法连接后端：请先运行 run_server.bat，再访问 http://localhost:8080";
    showMenu("无法连接后端");
  }
}

for (const card of modeCards) {
  card.onclick = () => {
    selectedMode = card.dataset.menuMode;
    updateMenuSelection();
  };
}

for (const card of levelCards) {
  card.onclick = () => {
    selectedAiLevel = Number(card.dataset.aiLevel);
    updateMenuSelection();
  };
}

startBtn.onclick = startGame;
newBtn.onclick = newGame;
backBtn.onclick = () => showMenu("选择对局模式");

updateMenuSelection();
showMenu();
