"""Local black-box interaction app for the Plant Pet AI rule engine."""

from __future__ import annotations

import json
import sys
import webbrowser
from argparse import ArgumentParser
from dataclasses import asdict
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from src.plant_pet_ai.state_engine import (  # noqa: E402
    DEFAULT_CONFIG,
    ENV_LABELS,
    LEAF_LABELS,
    LEAF_SCOPE_LABELS,
    MOOD_LABELS,
    analyze_plant,
)
from src.plant_pet_ai.leaf_classifier import predict_leaf_image  # noqa: E402
from src.plant_pet_ai.fixed_camera_state import (  # noqa: E402
    DEFAULT_CAMERA_CONFIG,
    VisionObservation,
    should_run_visual_check,
    stabilize_vision_status,
)
from src.plant_pet_ai.plant_status_yolo import predict_plant_status_yolo  # noqa: E402


FIXED_CONFIG = DEFAULT_CONFIG
FIXED_CAMERA_CONFIG = DEFAULT_CAMERA_CONFIG


def fixed_config_dict() -> dict:
    data = asdict(FIXED_CONFIG)
    data["env_penalty"] = dict(FIXED_CONFIG.env_penalty)
    data["leaf_penalty"] = dict(FIXED_CONFIG.leaf_penalty)
    data["fixed_camera"] = asdict(FIXED_CAMERA_CONFIG)
    return data


def state_to_dict(state) -> dict:
    return {
        "env_status": state.env_status,
        "env_label": ENV_LABELS[state.env_status],
        "leaf_status": state.leaf_status,
        "leaf_label": LEAF_LABELS[state.leaf_status],
        "leaf_scope": state.leaf_scope,
        "leaf_scope_label": LEAF_SCOPE_LABELS[state.leaf_scope],
        "comfort_score": state.comfort_score,
        "health_score": state.health_score,
        "mood": state.mood,
        "mood_label": MOOD_LABELS[state.mood],
        "face": state.face,
        "summary": state.summary,
        "advice": list(state.advice),
        "issues": list(state.issues),
        "care_notes": list(state.care_notes),
    }


def analyze_payload(payload: dict) -> dict:
    leaf_prediction = None
    plant_status_prediction = None
    fixed_camera_state = None
    visual_check = None
    leaf_status = str(payload.get("leaf_status", "healthy"))
    leaf_scope = str(payload.get("leaf_scope", "local"))

    vision_history = list(payload.get("vision_history", []))
    previous_plant_status = str(payload.get("stable_plant_status", "healthy_no_disease"))
    plant_image_path = payload.get("plant_image_path")
    direct_plant_status = payload.get("plant_status")

    if plant_image_path:
        plant_status_prediction = predict_plant_status_yolo(plant_image_path)
        plant_status_prediction["timestamp"] = _utc_now()
        vision_history.append(
            {
                "plant_status": plant_status_prediction["plant_status"],
                "confidence": plant_status_prediction["confidence"],
                "timestamp": plant_status_prediction["timestamp"],
                "image_path": plant_status_prediction["image_path"],
            }
        )
    elif direct_plant_status:
        plant_status_prediction = {
            "plant_status": str(direct_plant_status),
            "confidence": float(payload.get("plant_status_confidence", 1.0)),
            "timestamp": _utc_now(),
            "image_path": None,
        }
        vision_history.append(plant_status_prediction)

    if vision_history:
        fixed_camera_state = stabilize_vision_status(
            vision_history,
            previous_stable_status=previous_plant_status,
            config=FIXED_CAMERA_CONFIG,
        ).to_dict()
        leaf_status = fixed_camera_state["leaf_status"]
        leaf_scope = fixed_camera_state["leaf_scope"]

    schedule_status = (
        fixed_camera_state["stable_plant_status"]
        if fixed_camera_state
        else previous_plant_status
    )
    schedule_mode = (
        fixed_camera_state["mode"]
        if fixed_camera_state
        else str(payload.get("visual_mode", "stable"))
    )
    visual_check = should_run_visual_check(
        last_checked_at=payload.get("last_visual_checked_at"),
        stable_plant_status=schedule_status,
        mode=schedule_mode,
        light=float(payload["light"]) if "light" in payload else None,
        config=FIXED_CAMERA_CONFIG,
    )

    if payload.get("image_path"):
        leaf_prediction = predict_leaf_payload(payload)
        if not fixed_camera_state:
            leaf_status = leaf_prediction["leaf_status"]
            leaf_scope = leaf_prediction.get("leaf_scope", leaf_scope)

    reading = {
        "soil_moisture": float(payload.get("soil_moisture", 45)),
        "temperature": float(payload.get("temperature", 26)),
        "air_humidity": float(payload.get("air_humidity", 55)),
        "light": float(payload.get("light", 500)),
        "touched": bool(payload.get("touched", False)),
        "leaf_status": leaf_status,
        "leaf_scope": leaf_scope,
    }
    state = analyze_plant(**reading, config=FIXED_CONFIG)
    return {
        "reading": reading,
        "state": state_to_dict(state),
        "fixed_config": fixed_config_dict(),
        "leaf_prediction": leaf_prediction,
        "plant_status_prediction": plant_status_prediction,
        "fixed_camera_state": fixed_camera_state,
        "visual_check": {
            "should_run": visual_check[0],
            "reason": visual_check[1],
            "interval_minutes": visual_check[2],
        },
        "vision_history": vision_history[-12:],
    }


def predict_leaf_payload(payload: dict) -> dict:
    image_path = payload.get("image_path")
    if not image_path:
        raise ValueError("Missing image_path.")
    threshold = float(payload.get("healthy_threshold", 0.75))
    return predict_leaf_image(image_path, healthy_threshold=threshold)


def predict_plant_status_payload(payload: dict) -> dict:
    image_path = payload.get("plant_image_path") or payload.get("image_path")
    if not image_path:
        raise ValueError("Missing plant_image_path.")
    return predict_plant_status_yolo(
        image_path,
        device=str(payload.get("device", "cpu")),
    )


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


HTML = r"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Plant Pet AI 黑匣子</title>
  <style>
    :root {
      --bg: #f3f6f1;
      --panel: #ffffff;
      --ink: #1d2a22;
      --muted: #66736b;
      --line: #d7dfd5;
      --green: #2f7d4f;
      --green-soft: #e6f2eb;
      --blue-soft: #eef3fa;
      --blue: #284f7d;
      --amber-soft: #fff4dd;
      --amber: #7a4a12;
      --shadow: 0 14px 36px rgba(31, 42, 36, 0.08);
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Microsoft YaHei", "Segoe UI", Arial, sans-serif;
      color: var(--ink);
      background: var(--bg);
    }
    header {
      min-height: 150px;
      padding: 28px min(5vw, 56px);
      display: flex;
      align-items: end;
      justify-content: space-between;
      gap: 22px;
      color: white;
      background:
        linear-gradient(135deg, rgba(47, 125, 79, .96), rgba(47, 95, 152, .9)),
        radial-gradient(circle at 80% 20%, rgba(255, 255, 255, .22), transparent 32%);
    }
    h1, h2, h3, p { margin: 0; }
    h1 { font-size: 34px; letter-spacing: 0; margin-bottom: 8px; }
    header p { max-width: 780px; color: rgba(255, 255, 255, .88); line-height: 1.65; }
    .lock {
      border: 1px solid rgba(255,255,255,.3);
      background: rgba(255,255,255,.14);
      border-radius: 8px;
      padding: 12px 16px;
      min-width: 154px;
      text-align: center;
      line-height: 1.45;
    }
    main {
      padding: 24px min(5vw, 56px) 46px;
      display: grid;
      grid-template-columns: minmax(320px, 430px) 1fr;
      gap: 22px;
      align-items: start;
    }
    section {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 8px;
      box-shadow: var(--shadow);
    }
    .head {
      padding: 17px 20px 12px;
      border-bottom: 1px solid var(--line);
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 12px;
    }
    .content { padding: 18px 20px 20px; }
    .control { margin-bottom: 18px; }
    label {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      color: var(--muted);
      font-size: 14px;
      margin-bottom: 8px;
    }
    label strong { color: var(--ink); }
    input[type="range"] { width: 100%; accent-color: var(--green); }
    select {
      width: 100%;
      border: 1px solid var(--line);
      border-radius: 6px;
      padding: 10px;
      background: white;
      color: var(--ink);
      font: inherit;
    }
    .grid2 { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
    .button-row { display: flex; gap: 10px; flex-wrap: wrap; }
    button {
      border: 0;
      border-radius: 6px;
      padding: 10px 13px;
      background: var(--green);
      color: white;
      font: inherit;
      cursor: pointer;
    }
    button.secondary {
      background: #eef3ef;
      color: var(--ink);
      border: 1px solid var(--line);
    }
    .plant-stage {
      min-height: 430px;
      display: grid;
      grid-template-columns: 230px 1fr;
      gap: 24px;
      align-items: center;
      padding: 28px;
    }
    .avatar {
      height: 230px;
      border-radius: 8px;
      border: 1px solid #bfd4c5;
      background: linear-gradient(180deg, #f8fbf8 0%, #e7f3ec 100%);
      display: grid;
      place-items: center;
      position: relative;
      overflow: hidden;
    }
    .avatar::before {
      content: "";
      position: absolute;
      width: 80px;
      height: 112px;
      bottom: 28px;
      border-radius: 46px 46px 14px 14px;
      background: linear-gradient(180deg, #58a96d, #2f7d4f);
      box-shadow: inset -14px -10px 0 rgba(0,0,0,.08);
    }
    .avatar::after {
      content: "";
      position: absolute;
      width: 90px;
      height: 42px;
      bottom: 14px;
      border-radius: 50%;
      background: rgba(47, 125, 79, .16);
    }
    .face {
      position: relative;
      z-index: 1;
      font-size: 58px;
      transform: translateY(-24px);
      transition: transform .22s ease, filter .22s ease, opacity .22s ease;
    }
    .face.changed {
      transform: translateY(-30px) scale(1.08);
      filter: drop-shadow(0 8px 12px rgba(47, 125, 79, .22));
    }
    .avatar.touched .face { animation: touch-bounce .62s ease; }
    .avatar.buffering .face { opacity: .86; }
    @keyframes touch-bounce {
      0% { transform: translateY(-24px) scale(1); }
      34% { transform: translateY(-38px) scale(1.12); }
      68% { transform: translateY(-20px) scale(.98); }
      100% { transform: translateY(-24px) scale(1); }
    }
    .score {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-width: 78px;
      height: 44px;
      border-radius: 8px;
      background: var(--green-soft);
      color: var(--green);
      font-size: 24px;
      font-weight: 700;
    }
    .state-title {
      display: flex;
      align-items: center;
      gap: 12px;
      flex-wrap: wrap;
      margin-bottom: 12px;
    }
    .state-title h2 { font-size: 28px; letter-spacing: 0; }
    .summary { color: var(--muted); line-height: 1.7; margin-bottom: 16px; }
    .pill-row { display: flex; gap: 8px; flex-wrap: wrap; margin-bottom: 14px; }
    .pill {
      border-radius: 999px;
      padding: 5px 10px;
      background: #eef3ef;
      color: var(--muted);
      font-size: 13px;
    }
    .notice {
      border-radius: 8px;
      padding: 13px 14px;
      line-height: 1.65;
      margin-top: 12px;
    }
    .notice.issue { background: var(--amber-soft); color: var(--amber); }
    .notice.advice { background: var(--green-soft); color: #255f3c; }
    .notice.buffer { background: var(--blue-soft); color: var(--blue); }
    .param-grid {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      font-size: 13px;
      color: var(--muted);
    }
    .param {
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 10px;
      background: #fbfcfb;
    }
    .param strong { display: block; color: var(--ink); margin-bottom: 3px; }
    .right-stack { display: grid; gap: 22px; }
    .history-list {
      display: grid;
      gap: 10px;
      max-height: 360px;
      overflow: auto;
      padding-right: 4px;
    }
    .history-item {
      border: 1px solid var(--line);
      border-radius: 8px;
      padding: 11px 12px;
      display: grid;
      grid-template-columns: 44px 1fr;
      gap: 11px;
      align-items: start;
      background: #fbfcfb;
    }
    .history-face {
      width: 44px;
      height: 44px;
      display: grid;
      place-items: center;
      border-radius: 8px;
      background: var(--green-soft);
      font-size: 24px;
    }
    .history-title {
      display: flex;
      justify-content: space-between;
      gap: 12px;
      flex-wrap: wrap;
      font-size: 14px;
      color: var(--ink);
    }
    .history-time { color: var(--muted); font-size: 12px; }
    .history-meta {
      color: var(--muted);
      font-size: 12px;
      line-height: 1.55;
      margin-top: 4px;
    }
    @media (max-width: 980px) {
      header { align-items: start; flex-direction: column; }
      main { grid-template-columns: 1fr; }
      .plant-stage { grid-template-columns: 1fr; }
      .avatar { height: 190px; }
    }
  </style>
</head>
<body>
  <header>
    <div>
      <h1>Plant Pet AI 黑匣子</h1>
      <p>参数已固定。输入当前植株状态，系统会等待数据稳定后再切换基础表情；测试阶段缓冲时间较短。</p>
    </div>
    <div class="lock">测试缓冲<br><strong id="bufferLabel">5 秒</strong></div>
  </header>

  <main>
    <div>
      <section>
        <div class="head"><h2>植株状态输入</h2></div>
        <div class="content">
          <div class="grid2" id="inputs"></div>
          <div class="control">
            <label><strong>叶片状态</strong></label>
            <select id="leaf_status">
              <option value="healthy">健康</option>
              <option value="yellow_leaf">黄叶</option>
              <option value="leaf_spot">病斑</option>
              <option value="pest_damage">虫咬</option>
              <option value="wilted">萎蔫</option>
              <option value="unknown">未知</option>
            </select>
          </div>
          <div class="button-row">
            <button id="touchButton">摸摸它</button>
            <button id="healthyPreset">健康样例</button>
            <button class="secondary" id="stressPreset">复合异常样例</button>
          </div>
        </div>
      </section>

      <section style="margin-top:22px">
        <div class="head"><h2>固定参数</h2></div>
        <div class="content"><div class="param-grid" id="fixedParams"></div></div>
      </section>
    </div>

    <div class="right-stack">
      <section>
        <div class="plant-stage">
          <div class="avatar" id="avatar"><div class="face" id="face">😄</div></div>
          <div>
            <div class="state-title">
              <h2 id="moodTitle">开心</h2>
              <div class="score" id="score">100</div>
            </div>
            <div class="pill-row">
              <span class="pill" id="envPill">环境舒适</span>
              <span class="pill" id="leafPill">叶片健康</span>
              <span class="pill" id="moodPill">happy</span>
            </div>
            <p class="summary" id="summary">等待输入。</p>
            <div class="notice buffer" id="bufferNotice">状态已稳定。</div>
            <div class="notice issue" id="issues">问题：暂无明显异常。</div>
            <div class="notice advice" id="advice">建议：维持稳定浇水、光照和通风即可。</div>
          </div>
        </div>
      </section>

      <section>
        <div class="head">
          <h2>历史状态</h2>
          <span class="pill" id="historyCount">0 条</span>
        </div>
        <div class="content">
          <div class="history-list" id="historyList"></div>
        </div>
      </section>
    </div>
  </main>

  <script>
    const BUFFER_MS = 5000;
    const TOUCH_MS = 1600;
    const inputDefs = [
      ["soil_moisture", "土壤湿度", 0, 100, 1, 45],
      ["temperature", "温度", 0, 45, 1, 26],
      ["air_humidity", "空气湿度", 0, 100, 1, 55],
      ["light", "光照", 0, 1800, 10, 500]
    ];
    const reading = {
      soil_moisture: 45,
      temperature: 26,
      air_humidity: 55,
      light: 500,
      leaf_status: "healthy"
    };
    let committedReading = {...reading};
    let displayedState = null;
    let lastMood = "";
    let history = [];
    let stabilizeTimer = null;
    let countdownTimer = null;
    let touchReturnTimer = null;

    function buildSlider(parent, key, title, min, max, step, value) {
      const wrap = document.createElement("div");
      wrap.className = "control";
      wrap.innerHTML = `<label><strong>${title}</strong><span id="${key}-value">${value}</span></label>
        <input id="${key}" type="range" min="${min}" max="${max}" step="${step}" value="${value}">`;
      parent.appendChild(wrap);
      wrap.querySelector("input").addEventListener("input", event => {
        reading[key] = Number(event.target.value);
        document.getElementById(`${key}-value`).textContent = reading[key];
        scheduleStabilization();
      });
    }

    function setReading(next) {
      Object.assign(reading, next);
      inputDefs.forEach(([key]) => {
        document.getElementById(key).value = reading[key];
        document.getElementById(`${key}-value`).textContent = reading[key];
      });
      leaf_status.value = reading.leaf_status;
      scheduleStabilization();
    }

    function buildInputs() {
      const box = document.getElementById("inputs");
      inputDefs.forEach(def => buildSlider(box, ...def));
      leaf_status.addEventListener("change", event => {
        reading.leaf_status = event.target.value;
        scheduleStabilization();
      });
      touchButton.addEventListener("click", () => {
        if (touchReturnTimer) clearTimeout(touchReturnTimer);
        analyze({...committedReading, touched: true}, {temporary: true, historyType: "触摸反馈"});
        touchReturnTimer = setTimeout(() => {
          if (displayedState) renderState(displayedState, {temporaryReturn: true});
          touchReturnTimer = null;
        }, TOUCH_MS);
      });
      healthyPreset.addEventListener("click", () => setReading({
        soil_moisture: 45,
        temperature: 26,
        air_humidity: 55,
        light: 500,
        leaf_status: "healthy"
      }));
      stressPreset.addEventListener("click", () => setReading({
        soil_moisture: 18,
        temperature: 35,
        air_humidity: 82,
        light: 1300,
        leaf_status: "wilted"
      }));
    }

    function scheduleStabilization() {
      if (touchReturnTimer) {
        clearTimeout(touchReturnTimer);
        touchReturnTimer = null;
        if (displayedState) renderState(displayedState, {temporaryReturn: true});
      }
      if (stabilizeTimer) clearTimeout(stabilizeTimer);
      if (countdownTimer) clearInterval(countdownTimer);

      avatar.classList.add("buffering");
      const start = Date.now();
      updateBufferNotice(BUFFER_MS);
      countdownTimer = setInterval(() => {
        const remaining = Math.max(0, BUFFER_MS - (Date.now() - start));
        updateBufferNotice(remaining);
      }, 200);
      stabilizeTimer = setTimeout(() => {
        clearInterval(countdownTimer);
        countdownTimer = null;
        stabilizeTimer = null;
        committedReading = {...reading};
        analyze({...committedReading, touched: false}, {commit: true});
      }, BUFFER_MS);
    }

    function updateBufferNotice(remainingMs) {
      const seconds = Math.ceil(remainingMs / 1000);
      bufferNotice.textContent =
        `数据变化中，基础表情暂不切换。测试版将在 ${seconds} 秒稳定后重新测量。`;
    }

    function renderParams(config) {
      fixedParams.innerHTML = "";
      const rows = [
        ["土壤偏干", `< ${config.soil_dry_threshold}`],
        ["土壤过湿", `> ${config.soil_wet_threshold}`],
        ["光照不足", `< ${config.light_dark_threshold}`],
        ["光照过强", `> ${config.light_bright_threshold}`],
        ["温度偏低", `< ${config.temp_cold_threshold}`],
        ["温度偏高", `> ${config.temp_hot_threshold}`],
        ["空气偏干", `< ${config.air_dry_threshold}`],
        ["空气偏湿", `> ${config.air_humid_threshold}`]
      ];
      rows.forEach(([name, value]) => {
        const div = document.createElement("div");
        div.className = "param";
        div.innerHTML = `<strong>${name}</strong>${value}`;
        fixedParams.appendChild(div);
      });
    }

    async function analyze(payload, options = {}) {
      const response = await fetch("/api/analyze", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify(payload)
      });
      const data = await response.json();
      renderParams(data.fixed_config);
      if (options.commit) {
        displayedState = data.state;
        avatar.classList.remove("buffering");
        bufferNotice.textContent = "状态已稳定，基础表情已更新。";
        addHistory(data.state, data.reading, options.historyType || "稳定状态");
      }
      renderState(data.state, options);
      if (options.temporary) {
        addHistory(data.state, data.reading, options.historyType || "触摸反馈");
      }
    }

    function renderState(state, options = {}) {
      face.textContent = state.face;
      if (options.temporary) {
        avatar.classList.remove("touched");
        void avatar.offsetWidth;
        avatar.classList.add("touched");
        bufferNotice.textContent = "摸摸反馈中，稍后回到当前稳定表情。";
      }
      if (options.temporaryReturn && stabilizeTimer) {
        avatar.classList.add("buffering");
      }
      if (state.mood !== lastMood) {
        face.classList.remove("changed");
        void face.offsetWidth;
        face.classList.add("changed");
        lastMood = state.mood;
      }
      moodTitle.textContent = state.mood_label;
      moodPill.textContent = state.mood;
      score.textContent = state.health_score;
      envPill.textContent = state.env_label;
      leafPill.textContent = state.leaf_label;
      summary.textContent = state.summary;
      issues.textContent = state.issues.length ? `问题：${state.issues.join("；")}` : "问题：暂无明显异常。";
      advice.textContent = `建议：${state.advice.join(" ")}`;
    }

    function addHistory(state, readingSnapshot, type) {
      history.unshift({
        type,
        state,
        reading: {...readingSnapshot},
        time: new Date()
      });
      history = history.slice(0, 12);
      renderHistory();
    }

    function renderHistory() {
      historyCount.textContent = `${history.length} 条`;
      historyList.innerHTML = "";
      if (!history.length) {
        historyList.innerHTML = `<div class="history-meta">稳定提交或触摸后，会在这里记录最近状态。</div>`;
        return;
      }
      history.forEach(item => {
        const node = document.createElement("div");
        node.className = "history-item";
        const time = item.time.toLocaleTimeString("zh-CN", {hour12: false});
        const issueText = item.state.issues.length ? item.state.issues.join("；") : "无明显异常";
        node.innerHTML = `
          <div class="history-face">${item.state.face}</div>
          <div>
            <div class="history-title">
              <strong>${item.type} · ${item.state.mood_label}</strong>
              <span class="history-time">${time}</span>
            </div>
            <div class="history-meta">
              健康分 ${item.state.health_score}；${item.state.env_label}；${item.state.leaf_label}
            </div>
            <div class="history-meta">
              土壤 ${item.reading.soil_moisture}，温度 ${item.reading.temperature}，空气湿度 ${item.reading.air_humidity}，光照 ${item.reading.light}
            </div>
            <div class="history-meta">问题：${issueText}</div>
          </div>`;
        historyList.appendChild(node);
      });
    }

    buildInputs();
    bufferLabel.textContent = `${BUFFER_MS / 1000} 秒`;
    renderHistory();
    analyze({...committedReading, touched: false}, {commit: true, historyType: "初始状态"});
  </script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
        if self.path not in {"/", "/index.html"}:
            self.send_error(404)
            return
        self.respond(HTML.encode("utf-8"), "text/html; charset=utf-8")

    def do_POST(self) -> None:
        if self.path not in {
            "/api/analyze",
            "/api/predict_leaf",
            "/api/predict_plant_status",
        }:
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length) or b"{}")
        try:
            if self.path == "/api/predict_leaf":
                result = predict_leaf_payload(payload)
            elif self.path == "/api/predict_plant_status":
                result = predict_plant_status_payload(payload)
            else:
                result = analyze_payload(payload)
        except Exception as exc:
            self.respond_json({"error": str(exc)}, status=400)
            return
        self.respond_json(result)

    def log_message(self, format: str, *args) -> None:
        return

    def respond(self, body: bytes, content_type: str) -> None:
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def respond_json(self, payload: dict, status: int = 200) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main() -> None:
    parser = ArgumentParser(description="Run the Plant Pet AI black-box app.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", default=8765, type=int)
    parser.add_argument("--no-open", action="store_true")
    args = parser.parse_args()

    server = ThreadingHTTPServer((args.host, args.port), Handler)
    url = f"http://{args.host}:{args.port}"
    print(f"Plant Pet AI black-box app: {url}")
    if not args.no_open:
        try:
            webbrowser.open(url)
        except webbrowser.Error:
            pass
    server.serve_forever()


if __name__ == "__main__":
    main()
