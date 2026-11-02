"""Input-length and dtype validation on /predict and /predict_batch."""
from __future__ import annotations

import pytest


@pytest.mark.parametrize("n", [0, 1, 47, 49, 96])
def test_wrong_length_rejected(client, n):
    r = client.post("/predict", json={"run_id": 1, "window": [10.0] * n})
    assert r.status_code == 422


@pytest.mark.parametrize(
    "bad",
    [["10.5"] * 48, [True] * 48, [None] * 48, [[1.0]] * 48],
    ids=["strings", "booleans", "nulls", "nested"],
)
def test_wrong_dtype_rejected(client, bad):
    assert client.post("/predict", json={"run_id": 1, "window": bad}).status_code == 422


@pytest.mark.parametrize("v", [-61.0, 61.0, 1e9])
def test_out_of_range_rejected(client, v):
    assert client.post("/predict", json={"run_id": 1, "window": [10.0] * 47 + [v]}).status_code == 422


def test_batch_limits(client):
    assert client.post("/predict_batch", json={"run_id": 1, "windows": []}).status_code == 422
    assert client.post("/predict_batch", json={"run_id": 1, "windows": [[1.0] * 48] * 51}).status_code == 422
    assert client.post("/predict_batch", json={"run_id": 1, "windows": [[1.0] * 47]}).status_code == 422


def test_unknown_run_is_404_not_500(client, good_window):
    assert client.post("/predict", json={"run_id": 999, "window": good_window}).status_code == 404


def test_integers_are_accepted_as_numbers(client, trained):
    r = client.post("/predict", json={"run_id": trained["attention"], "window": [5] * 48})
    assert r.status_code == 200
