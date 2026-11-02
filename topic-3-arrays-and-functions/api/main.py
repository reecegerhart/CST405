"""FastAPI model service -- Cloud #2 (deployed on Render.com).

Hosts the trained forecasters. Reads training rows from Supabase, writes run rows
and artifacts back, and logs EVERY served prediction (hash + forecast + attention
vector) to the `predictions` table so a weekly review can be run.
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
from typing import List

import torch
from fastapi import FastAPI, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware

from api import db
from api.training import predict_windows, train_model
from shared.schemas import (
    Health,
    PredictBatchRequest,
    PredictBatchResponse,
    PredictRequest,
    PredictResponse,
    Run,
    StoredPrediction,
    TrainRequest,
    TrainResponse,
    Version,
)

app = FastAPI(
    title="Forecast-It API",
    description="24-hour temperature forecasts: LSTM baseline vs. self-attention, with an audit trail.",
    version="1.0.0",
)
app.add_middleware(
    CORSMiddleware,
    allow_origins=os.environ.get("ALLOWED_ORIGINS", "*").split(","),
    allow_methods=["*"],
    allow_headers=["*"],
)


def _git_sha() -> str:
    if os.environ.get("RENDER_GIT_COMMIT"):
        return os.environ["RENDER_GIT_COMMIT"][:7]
    try:
        return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()
    except Exception:
        return "unknown"


def _project_ref():
    url = os.environ.get("SUPABASE_URL", "")
    return url.split("//", 1)[1].split(".", 1)[0] if url.startswith("https://") else None


def _input_hash(window: List[float]) -> str:
    """Stable hash of the window. Raw temperatures are never stored."""
    return hashlib.sha256(json.dumps([round(float(v), 4) for v in window]).encode()).hexdigest()


def _artifact_or_404(run_id: int) -> str:
    b64 = db.get_run_artifact(run_id)
    if b64 is None:
        raise HTTPException(status_code=404, detail="run_id not found")
    return b64


def _response(run_id: int, window, result: dict) -> PredictResponse:
    return PredictResponse(run_id=run_id, input_hash=_input_hash(window), **result)


# ---------------------------------------------------------------------------
# training
# ---------------------------------------------------------------------------
@app.post("/train", response_model=TrainResponse, tags=["training"])
def train(req: TrainRequest) -> TrainResponse:
    """Read train/val/test rows from Supabase, fit one model, persist the run."""
    splits = {s: db.fetch_split(s, req.max_train_rows) for s in ("train", "val", "test")}
    if min(len(v) for v in splits.values()) < 200:
        raise HTTPException(status_code=409, detail="Corpus is empty or too small. Run db/load.py first.")
    hp = req.model_dump(exclude={"model_type", "max_train_rows"})
    out = train_model(
        splits["train"], splits["val"], splits["test"], req.model_type,
        lr=req.lr, batch_size=req.batch_size, epochs=req.epochs, hidden_dim=req.hidden_dim,
    )
    run = db.insert_run(req.model_type, hp, out["metrics"], out["examples"], out["model_b64"])
    return TrainResponse(run=Run(**{k: run[k] for k in Run.model_fields}))


@app.get("/runs", response_model=List[Run], tags=["training"])
def list_runs() -> List[Run]:
    return [Run(**{k: r[k] for k in Run.model_fields}) for r in db.latest_runs(50)]


# ---------------------------------------------------------------------------
# prediction
# ---------------------------------------------------------------------------
@app.post("/predict", response_model=PredictResponse, tags=["prediction"])
def predict(req: PredictRequest) -> PredictResponse:
    """Forecast 24 h ahead; return the attention matrix; log the prediction."""
    window = [float(v) for v in req.window]
    result = predict_windows(_artifact_or_404(req.run_id), [window])[0]
    resp = _response(req.run_id, window, result)
    db.insert_prediction(resp.input_hash, resp.prediction, resp.attention_last_row, req.run_id)
    return resp


@app.post("/predict_batch", response_model=PredictBatchResponse, tags=["prediction"])
def predict_batch(req: PredictBatchRequest) -> PredictBatchResponse:
    windows = [[float(v) for v in w] for w in req.windows]
    results = predict_windows(_artifact_or_404(req.run_id), windows)
    resps = [_response(req.run_id, w, r) for w, r in zip(windows, results)]
    db.insert_predictions(
        [
            {
                "input_hash": r.input_hash,
                "predicted_value": r.prediction,
                "attention_vector_json": r.attention_last_row,
                "served_by_run_id": req.run_id,
            }
            for r in resps
        ]
    )
    return PredictBatchResponse(results=resps)


@app.get("/predictions", response_model=List[StoredPrediction], tags=["prediction"])
def predictions(limit: int = Query(50, ge=1, le=500)) -> List[StoredPrediction]:
    rows = db.recent_predictions(limit)
    return [StoredPrediction(**{k: r[k] for k in StoredPrediction.model_fields}) for r in rows]


# ---------------------------------------------------------------------------
# ops
# ---------------------------------------------------------------------------
@app.get("/healthz", response_model=Health, tags=["ops"])
def healthz() -> Health:
    supabase_ok = db.ping()
    model_ok = torch.tensor([1.0]).sum().item() == 1.0
    return Health(status="ok" if (supabase_ok and model_ok) else "degraded",
                  model_loader=model_ok, supabase=supabase_ok)


@app.get("/version", response_model=Version, tags=["ops"])
def version() -> Version:
    return Version(git_sha=_git_sha(), torch_version=torch.__version__, supabase_project_ref=_project_ref())
