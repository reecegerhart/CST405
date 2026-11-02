"""Streamlit UI -- Cloud #1 (Streamlit Community Cloud). A thin client: no torch, no model code.

  * forecasts and attention weights come from the FastAPI service over HTTPS,
  * the only direct database access is READ-ONLY, using the Supabase anon key
    (runs, predictions, sequences). Writes happen server-side only.

st.secrets: API_URL, SUPABASE_URL, SUPABASE_ANON_KEY  (see .streamlit/secrets.toml.example)
"""
from __future__ import annotations

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import requests
import streamlit as st
from supabase import create_client

WINDOW, HORIZON = 48, 24
API_URL = st.secrets["API_URL"].rstrip("/")

st.set_page_config(page_title="Forecast-It", page_icon="🌡️", layout="wide")
st.title("🌡️ Forecast-It — 24-Hour Temperature Forecasts with Attention")
st.caption("Streamlit (this UI) → FastAPI on Render (LSTM vs. self-attention) → Supabase (corpus + audit trail).")


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------
@st.cache_resource
def sb():
    return create_client(st.secrets["SUPABASE_URL"], st.secrets["SUPABASE_ANON_KEY"])


def api_post(path: str, payload: dict):
    r = requests.post(f"{API_URL}{path}", json=payload, timeout=120)
    if r.status_code == 422:
        st.error("The API rejected that input: " + str(r.json().get("detail"))[:300])
        return None
    r.raise_for_status()
    return r.json()


@st.cache_data(ttl=30)
def load_runs() -> pd.DataFrame:
    rows = sb().table("runs").select("*").order("created_at", desc=True).limit(50).execute().data
    return pd.DataFrame(rows)


def latest_run(runs: pd.DataFrame, model_type: str):
    sub = runs[runs["model_type"] == model_type] if not runs.empty else runs
    return None if sub.empty else sub.iloc[0].to_dict()


def heatmap(matrix, title: str):
    fig, ax = plt.subplots(figsize=(5.2, 4.4))
    im = ax.imshow(np.asarray(matrix), cmap="viridis", aspect="auto")
    ax.set_xlabel("attended-to hour (0 = oldest, 47 = most recent)")
    ax.set_ylabel("query hour")
    ax.set_title(title)
    fig.colorbar(im, ax=ax, label="attention weight")
    fig.tight_layout()
    return fig


def attention_bar(row, title: str = "Which past hours the forecast looked at"):
    fig, ax = plt.subplots(figsize=(7, 2.4))
    hours_ago = np.arange(WINDOW)[::-1]
    ax.bar(hours_ago, row, color="#2a6f97")
    ax.invert_xaxis()
    ax.set_xlabel("hours before the forecast was made")
    ax.set_ylabel("weight")
    ax.set_title(title)
    fig.tight_layout()
    return fig


def window_plot(window, target=None, preds: dict | None = None, title: str = ""):
    fig, ax = plt.subplots(figsize=(7, 2.8))
    ax.plot(range(-WINDOW + 1, 1), window, color="#444", label="last 48 h (input)")
    if target is not None:
        ax.scatter([HORIZON], [target], color="black", zorder=3, label="actual +24 h")
    for name, (v, color) in (preds or {}).items():
        ax.scatter([HORIZON], [v], color=color, marker="x", s=70, zorder=3, label=f"{name} forecast")
    ax.set_xlabel("hours relative to forecast time")
    ax.set_ylabel("°C")
    ax.set_title(title)
    ax.legend(fontsize=7, ncol=2)
    fig.tight_layout()
    return fig


tabs = st.tabs(["Concepts", "Try It", "Baseline vs. Attention", "Recent Predictions", "Failure Gallery", "Model Card"])

# ---------------------------------------------------------------------------
# Concepts
# ---------------------------------------------------------------------------
with tabs[0]:
    st.header("From a recurrent step to scaled dot-product attention")
    st.markdown(
        "The input is 48 hourly temperatures $x_1,\\dots,x_{48}$. Each hour is first embedded into a "
        "vector (plus a sinusoidal position code, because attention by itself cannot tell hour 3 from hour 40). "
        "Call the resulting matrix $X \\in \\mathbb{R}^{48\\times d}$."
    )
    st.markdown("**1. Three learned projections** turn every hour into a query, a key and a value:")
    st.latex(r"Q = XW_Q,\qquad K = XW_K,\qquad V = XW_V")
    st.markdown("**2. Compatibility scores.** Hour $i$ asks a question ($q_i$); hour $j$ advertises what it holds ($k_j$). "
                "Their dot product measures how well they match:")
    st.latex(r"S_{ij} = \frac{q_i \cdot k_j}{\sqrt{d_k}}")
    st.markdown("The $\\sqrt{d_k}$ keeps the scores from growing with dimension. Without it the softmax saturates and its gradients vanish.")
    st.markdown("**3. Softmax over each row** turns scores into weights that sum to one:")
    st.latex(r"A_{ij} = \frac{e^{S_{ij}}}{\sum_{k} e^{S_{ik}}}")
    st.markdown("**4. Weighted sum of values**, and the whole block in one line:")
    st.latex(r"\mathrm{Attention}(Q,K,V) = \mathrm{softmax}\!\left(\frac{QK^{\top}}{\sqrt{d_k}}\right)V")
    st.markdown("The forecast is read from the **last hour's output row**, so row 48 of $A$ shows which past hours drove it.")
    st.subheader("Recurrent step vs. attention step")
    st.table(
        pd.DataFrame(
            {
                "": ["Core operation", "Path from hour 1 to the output", "Parallel over time?", "Order information",
                     "Cost per window", "Built-in explanation"],
                "LSTM (recurrent)": [
                    r"h_t = f(h_{t-1}, x_t), gated",
                    "47 sequential steps (gradient must survive them)",
                    "No: step t waits for step t-1",
                    "Implicit in the order of updates",
                    "O(T · d²)",
                    "None (hidden states are opaque)",
                ],
                "Self-attention": [
                    r"softmax(QKᵀ/√d)V, all pairs at once",
                    "1 step (any hour talks directly to any hour)",
                    "Yes: one matrix multiply",
                    "Must be added (positional encoding)",
                    "O(T² · d)",
                    "The 48×48 weight matrix A",
                ],
            }
        ).set_index("")
    )

# ---------------------------------------------------------------------------
# Try It
# ---------------------------------------------------------------------------
with tabs[1]:
    st.header("Try it")
    runs = load_runs()
    if runs.empty:
        st.info("No trained runs yet. Run `python -m api.train_cli` to train both models.")
    else:
        labels = {int(r.id): f"run {int(r.id)} · {r.model_type} · MAE {r.test_mae:.2f}°C" for r in runs.itertuples()}
        default = latest_run(runs, "attention")
        run_id = st.selectbox("Model run", list(labels), format_func=labels.get,
                              index=list(labels).index(int(default["id"])) if default else 0)

        if st.button("Load a real 48-hour window from the test set"):
            rows = (sb().table("sequences").select("value").eq("split", "test")
                    .order("ts", desc=True).limit(2000).execute().data)
            vals = [r["value"] for r in rows][::-1]
            if len(vals) >= WINDOW:
                start = int(np.random.default_rng().integers(0, len(vals) - WINDOW + 1))
                st.session_state["window_text"] = ", ".join(f"{v:.1f}" for v in vals[start : start + WINDOW])
        text = st.text_area("Last 48 hourly temperatures in °C, comma-separated, oldest first",
                            key="window_text", height=110)

        if st.button("Forecast +24 h", type="primary"):
            try:
                window = [float(t) for t in text.replace("\n", " ").split(",") if t.strip()]
            except ValueError:
                window = None
                st.error("Could not parse the numbers. Use comma-separated values like 4.2, 4.0, ...")
            if window is not None and len(window) != WINDOW:
                st.error(f"Need exactly {WINDOW} values; got {len(window)}.")
            elif window is not None:
                resp = api_post("/predict", {"run_id": int(run_id), "window": window})
                if resp:
                    c1, c2 = st.columns([1, 2])
                    c1.metric("Forecast, 24 h ahead", f"{resp['prediction']:.1f} °C")
                    c1.caption(f"≈90% band: {resp['interval_low']:.1f} to {resp['interval_high']:.1f} °C "
                               f"({resp['interval_note'].lower()})")
                    c1.caption(f"Logged to Supabase as hash {resp['input_hash'][:12]}…")
                    c2.pyplot(window_plot(window, preds={resp["model_type"]: (resp["prediction"], "#c1121f")},
                                          title="Input window and forecast"))
                    if resp["attention_matrix"]:
                        h1, h2 = st.columns(2)
                        h1.pyplot(heatmap(resp["attention_matrix"], "Self-attention matrix (48×48)"))
                        h2.pyplot(attention_bar(resp["attention_last_row"]))
                    else:
                        st.info("The LSTM baseline has no attention weights. Pick an attention run to see which hours mattered.")

# ---------------------------------------------------------------------------
# Baseline vs. Attention
# ---------------------------------------------------------------------------
with tabs[2]:
    st.header("Baseline vs. attention")
    runs = load_runs()
    lstm, attn = latest_run(runs, "lstm"), latest_run(runs, "attention")
    if not (lstm and attn):
        st.info("Train both models first (`python -m api.train_cli`).")
    else:
        st.table(pd.DataFrame(
            {
                "Test MAE (°C)": [lstm["test_mae"], attn["test_mae"], lstm["persistence_mae"]],
                "Test MSE": [lstm["test_mse"], attn["test_mse"], np.nan],
                "Parameters": [lstm["n_params"], attn["n_params"], 0],
                "Train time (s)": [lstm["train_seconds"], attn["train_seconds"], 0],
            },
            index=[f"LSTM (run {int(lstm['id'])})", f"Attention (run {int(attn['id'])})", "Persistence (tomorrow = now)"],
        ).round(3))
        st.caption("Same data, scaler, optimizer, loss, epoch budget and seed for both models. Values come from the `runs` table.")
        st.subheader("Three illustrative test inputs")
        cols = st.columns(3)
        for i, col in enumerate(cols):
            a_ex, l_ex = attn["examples"]["illustrative"][i], lstm["examples"]["illustrative"][i]
            col.pyplot(window_plot(a_ex["window"], a_ex["target"],
                                   {"LSTM": (l_ex["prediction"], "#e07a1f"), "Attention": (a_ex["prediction"], "#c1121f")},
                                   f"Example {i + 1}"))
            col.caption(f"Actual {a_ex['target']:.1f}°C · LSTM {l_ex['prediction']:.1f} (err {l_ex['abs_error']:.1f}) · "
                        f"Attention {a_ex['prediction']:.1f} (err {a_ex['abs_error']:.1f}). "
                        f"Attention peaked {a_ex['attention_peak_hours_ago']} h back.")
            col.pyplot(attention_bar(a_ex["attention"], "Attention (last row)"))

# ---------------------------------------------------------------------------
# Recent Predictions
# ---------------------------------------------------------------------------
with tabs[3]:
    st.header("Recent predictions")
    st.caption("Read directly from Supabase with the anon key. Only a hash of each input is stored, never the raw readings.")
    try:
        rows = (sb().table("predictions").select("*").order("created_at", desc=True).limit(100).execute().data)
    except Exception as exc:  # noqa: BLE001
        rows, _ = [], st.error(f"Could not read predictions: {exc}")
    if not rows:
        st.info("No predictions yet. Make one on the Try It tab.")
    else:
        df = pd.DataFrame(rows)
        show = df.assign(input_hash=df["input_hash"].str[:12] + "…", has_attention=df["attention_vector_json"].notna())
        st.dataframe(show[["id", "created_at", "served_by_run_id", "predicted_value", "has_attention", "input_hash"]],
                     use_container_width=True, hide_index=True)   # click a header to sort
        with_attn = df[df["attention_vector_json"].notna()]
        if not with_attn.empty:
            pick = st.selectbox("Drill down: prediction id", with_attn["id"].tolist())
            row = with_attn[with_attn["id"] == pick].iloc[0]
            st.pyplot(heatmap([row["attention_vector_json"]], f"Attention for prediction {pick} ({row['predicted_value']:.1f} °C)"))

# ---------------------------------------------------------------------------
# Failure Gallery
# ---------------------------------------------------------------------------
with tabs[4]:
    st.header("Failure gallery: confidently wrong")
    attn = latest_run(load_runs(), "attention")
    if not attn or not (attn.get("examples") or {}).get("failures"):
        st.info("Train the attention model first.")
    else:
        st.markdown(
            "A regressor has no built-in confidence, so we define one from the attention itself: a forecast is "
            "**confident** when its attention is sharply focused (entropy in the lowest 20% of the test set). "
            "These are the three most-wrong forecasts among those confident ones."
        )
        for i, f in enumerate(attn["examples"]["failures"], 1):
            c1, c2 = st.columns(2)
            c1.pyplot(window_plot(f["window"], f["target"], {"Attention": (f["prediction"], "#c1121f")}, f"Failure {i}"))
            c2.pyplot(attention_bar(f["attention"], f"Attention (entropy {f['attention_entropy']})"))
            move = f["target"] - f["window"][-1]
            st.markdown(
                f"**Failure {i}.** Forecast {f['prediction']:.1f}°C, actual {f['target']:.1f}°C (off by {f['abs_error']:.1f}°C). "
                f"The temperature actually moved {move:+.1f}°C from the last observed hour, and the model focused most on the reading "
                f"{f['attention_peak_hours_ago']} h back. Sharp focus on a few hours means the model extrapolates from the daily/trend "
                f"pattern in that slice; it cannot see the weather front, pressure change or cloud cover that actually drove the miss, "
                f"because the only input is past temperature."
            )
        st.markdown(
            "**Takeaway.** Peaked attention reads as certainty but is not a probability. It says *where the model looked*, "
            "not *how likely it is to be right*. That is why the app also shows an empirical error band and a persistence baseline."
        )

# ---------------------------------------------------------------------------
# Model Card
# ---------------------------------------------------------------------------
with tabs[5]:
    try:
        with open("MODEL_CARD.md", "r", encoding="utf-8") as fh:
            st.markdown(fh.read())
    except FileNotFoundError:
        st.warning("MODEL_CARD.md not found (the Streamlit app's working directory must be the repo root).")
