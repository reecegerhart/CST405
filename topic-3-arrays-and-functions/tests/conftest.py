"""Shared pytest fixtures.

Everything runs OFFLINE: `api.db` is replaced by an in-memory fake that mimics the
Supabase tables (sequences/runs/artifacts/predictions) closely enough to check the
joins. The live-Supabase test is skipped unless real credentials are in the env.
"""
from __future__ import annotations

import os

import numpy as np
import pytest

from shared.data import chronological_split_labels, make_synthetic_series


def has_supabase_creds() -> bool:
    return bool(os.environ.get("SUPABASE_URL") and os.environ.get("SUPABASE_SERVICE_KEY"))


@pytest.fixture
def fake_store():
    return {"runs": {}, "artifacts": {}, "predictions": [], "n_runs": 0}


@pytest.fixture(scope="session")
def small_corpus():
    series = make_synthetic_series(3000, seed=1)
    labels = chronological_split_labels(len(series))
    return {s: series[labels == s] for s in ("train", "val", "test")}


@pytest.fixture
def client(monkeypatch, fake_store, small_corpus):
    from fastapi.testclient import TestClient

    from api import db
    from api.main import app

    def insert_run(model_type, hyperparameters, metrics, examples, model_b64):
        fake_store["n_runs"] += 1
        row = {"id": fake_store["n_runs"], "model_type": model_type, "hyperparameters": hyperparameters,
               **{k: metrics[k] for k in ("test_mae", "test_mse", "val_mae", "persistence_mae",
                                          "resid_q90", "n_params", "train_seconds")},
               "examples": examples, "created_at": "2026-01-01T00:00:00+00:00"}
        fake_store["runs"][row["id"]] = row
        fake_store["artifacts"][row["id"]] = model_b64
        return row

    def insert_prediction(input_hash, predicted_value, attention_vector, run_id):
        row = {"id": len(fake_store["predictions"]) + 1, "input_hash": input_hash,
               "predicted_value": predicted_value, "attention_vector_json": attention_vector,
               "served_by_run_id": run_id, "created_at": "2026-01-01T00:00:00+00:00"}
        fake_store["predictions"].append(row)
        return row

    monkeypatch.setattr(db, "ping", lambda: True)
    monkeypatch.setattr(db, "fetch_split", lambda split, max_rows=None: small_corpus[split][:max_rows])
    monkeypatch.setattr(db, "insert_run", insert_run)
    monkeypatch.setattr(db, "get_run", lambda rid: fake_store["runs"].get(rid))
    monkeypatch.setattr(db, "get_run_artifact", lambda rid: fake_store["artifacts"].get(rid))
    monkeypatch.setattr(db, "latest_runs", lambda limit=50: list(fake_store["runs"].values())[::-1][:limit])
    monkeypatch.setattr(db, "insert_prediction", insert_prediction)
    monkeypatch.setattr(db, "insert_predictions",
                        lambda rows: [insert_prediction(r["input_hash"], r["predicted_value"],
                                                        r["attention_vector_json"], r["served_by_run_id"]) for r in rows])
    monkeypatch.setattr(db, "recent_predictions", lambda limit=50: fake_store["predictions"][::-1][:limit])

    with TestClient(app) as c:
        c._store = fake_store
        yield c


@pytest.fixture
def trained(client):
    """Train a tiny LSTM and a tiny attention model through the real /train endpoint."""
    ids = {}
    for mt in ("lstm", "attention"):
        r = client.post("/train", json={"model_type": mt, "epochs": 2, "batch_size": 64, "hidden_dim": 16})
        assert r.status_code == 200, r.text
        ids[mt] = r.json()["run"]["id"]
    return ids


@pytest.fixture
def good_window():
    return [float(v) for v in make_synthetic_series(200, seed=3)[50:98]]
