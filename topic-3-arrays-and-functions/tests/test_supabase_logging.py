"""/predict logs a row in `predictions` that joins to its `runs` row.

The offline test checks the join against the in-memory fake. The live test (skipped
without credentials) does the real thing with a PostgREST embedded join.
"""
from __future__ import annotations

import hashlib
import json

import pytest

from tests.conftest import has_supabase_creds


def test_predict_logs_row_joined_to_run(client, trained, good_window):
    run_id = trained["attention"]
    body = client.post("/predict", json={"run_id": run_id, "window": good_window}).json()

    rows = client._store["predictions"]
    assert len(rows) == 1
    row = rows[0]
    joined_run = client._store["runs"][row["served_by_run_id"]]          # the join
    assert joined_run["id"] == run_id and joined_run["model_type"] == "attention"
    assert row["predicted_value"] == pytest.approx(body["prediction"])
    assert len(row["attention_vector_json"]) == 48
    # privacy: only a hash is stored, never the readings
    expected = hashlib.sha256(json.dumps([round(v, 4) for v in good_window]).encode()).hexdigest()
    assert row["input_hash"] == expected
    assert "window" not in row

    listed = client.get("/predictions?limit=5").json()
    assert listed[0]["served_by_run_id"] == run_id


def test_batch_logs_every_row(client, trained, good_window):
    client.post("/predict_batch", json={"run_id": trained["lstm"], "windows": [good_window] * 4})
    assert len(client._store["predictions"]) == 4
    assert all(p["attention_vector_json"] is None for p in client._store["predictions"])   # lstm


@pytest.mark.skipif(not has_supabase_creds(), reason="No live Supabase credentials in environment.")
def test_live_predict_row_joins_to_runs_row(good_window):
    from fastapi.testclient import TestClient

    from api import db
    from api.main import app

    with TestClient(app) as c:
        runs = c.get("/runs").json()
        assert runs, "train a model first: python -m api.train_cli"
        run_id = runs[0]["id"]
        resp = c.post("/predict", json={"run_id": run_id, "window": good_window})
        assert resp.status_code == 200
        rows = (db.get_client().table("predictions").select("*, runs(id, model_type)")
                .eq("input_hash", resp.json()["input_hash"]).order("created_at", desc=True).limit(1).execute().data)
        assert rows and rows[0]["runs"]["id"] == run_id
