"""Supabase persistence helpers for the API tier (service-role key, server-side only).

All Supabase access for the model service goes through this module. Raw input
windows are never stored -- only their SHA-256 (see `predictions.input_hash`).

Environment variables (a local .env, and the Render dashboard):
    SUPABASE_URL          -> https://<project-ref>.supabase.co
    SUPABASE_SERVICE_KEY  -> the service-role key (secret!)
"""
from __future__ import annotations

import os
from typing import Dict, List, Optional

import numpy as np
from supabase import Client, create_client

_client: Optional[Client] = None
PAGE = 1000   # PostgREST returns at most 1000 rows per request

_RUN_COLS = (
    "id,model_type,hyperparameters,test_mae,test_mse,val_mae,persistence_mae,"
    "resid_q90,n_params,train_seconds,created_at"
)


def get_client() -> Client:
    global _client
    if _client is None:
        _client = create_client(os.environ["SUPABASE_URL"], os.environ["SUPABASE_SERVICE_KEY"])
    return _client


def ping() -> bool:
    try:
        get_client().table("runs").select("id").limit(1).execute()
        return True
    except Exception:
        return False


# ---------------------------------------------------------------------------
# corpus
# ---------------------------------------------------------------------------
def fetch_split(split: str, max_rows: Optional[int] = None) -> np.ndarray:
    """Read one split's temperatures from Supabase, in time order (paged)."""
    out: List[float] = []
    start = 0
    while True:
        resp = (
            get_client()
            .table("sequences")
            .select("value")
            .eq("split", split)
            .order("ts")
            .range(start, start + PAGE - 1)
            .execute()
        )
        out.extend(r["value"] for r in resp.data)
        if len(resp.data) < PAGE or (max_rows and len(out) >= max_rows):
            break
        start += PAGE
    return np.asarray(out[:max_rows] if max_rows else out, dtype=np.float32)


# ---------------------------------------------------------------------------
# runs + artifacts
# ---------------------------------------------------------------------------
def insert_run(model_type: str, hyperparameters: dict, metrics: Dict[str, float],
               examples: dict, model_b64: str) -> dict:
    client = get_client()
    run = (
        client.table("runs")
        .insert(
            {
                "model_type": model_type,
                "hyperparameters": hyperparameters,
                "test_mae": metrics["test_mae"],
                "test_mse": metrics["test_mse"],
                "val_mae": metrics["val_mae"],
                "persistence_mae": metrics["persistence_mae"],
                "resid_q90": metrics["resid_q90"],
                "n_params": metrics["n_params"],
                "train_seconds": metrics["train_seconds"],
                "examples": examples,
            }
        )
        .execute()
        .data[0]
    )
    client.table("run_artifacts").insert({"run_id": run["id"], "model_b64": model_b64}).execute()
    return run


def get_run(run_id: int) -> Optional[dict]:
    r = get_client().table("runs").select(_RUN_COLS).eq("id", run_id).limit(1).execute()
    return r.data[0] if r.data else None


def get_run_artifact(run_id: int) -> Optional[str]:
    r = get_client().table("run_artifacts").select("model_b64").eq("run_id", run_id).limit(1).execute()
    return r.data[0]["model_b64"] if r.data else None


def latest_runs(limit: int = 50) -> List[dict]:
    return (
        get_client().table("runs").select(_RUN_COLS).order("created_at", desc=True).limit(limit).execute().data
    )


# ---------------------------------------------------------------------------
# predictions (audit trail)
# ---------------------------------------------------------------------------
def insert_prediction(input_hash: str, predicted_value: float,
                      attention_vector: Optional[List[float]], run_id: int) -> dict:
    return (
        get_client()
        .table("predictions")
        .insert(
            {
                "input_hash": input_hash,
                "predicted_value": predicted_value,
                "attention_vector_json": attention_vector,
                "served_by_run_id": run_id,
            }
        )
        .execute()
        .data[0]
    )


def insert_predictions(rows: List[dict]) -> List[dict]:
    return get_client().table("predictions").insert(rows).execute().data


def recent_predictions(limit: int = 50) -> List[dict]:
    return (
        get_client()
        .table("predictions")
        .select("id,input_hash,predicted_value,attention_vector_json,served_by_run_id,created_at")
        .order("created_at", desc=True)
        .limit(limit)
        .execute()
        .data
    )
