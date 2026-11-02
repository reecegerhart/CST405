"""The attention matrix returned by /predict has the expected shape and properties."""
from __future__ import annotations

import numpy as np
import torch

from api.models import ScaledDotProductSelfAttention
from shared.data import WINDOW


def test_predict_returns_48x48_attention(client, trained, good_window):
    r = client.post("/predict", json={"run_id": trained["attention"], "window": good_window})
    assert r.status_code == 200
    body = r.json()
    a = np.array(body["attention_matrix"])
    assert a.shape == (WINDOW, WINDOW)
    assert np.allclose(a.sum(axis=1), 1.0, atol=1e-4)          # each row is a distribution
    assert (a >= 0).all()
    assert np.allclose(body["attention_last_row"], a[-1])
    assert body["interval_low"] < body["prediction"] < body["interval_high"]


def test_lstm_has_no_attention(client, trained, good_window):
    body = client.post("/predict", json={"run_id": trained["lstm"], "window": good_window}).json()
    assert body["attention_matrix"] is None and body["attention_last_row"] is None
    assert body["model_type"] == "lstm"


def test_batch_matches_single(client, trained, good_window):
    single = client.post("/predict", json={"run_id": trained["attention"], "window": good_window}).json()
    batch = client.post("/predict_batch", json={"run_id": trained["attention"], "windows": [good_window] * 3}).json()
    assert len(batch["results"]) == 3
    assert abs(batch["results"][0]["prediction"] - single["prediction"]) < 1e-4


def test_handwritten_attention_matches_the_formula():
    """softmax(QK^T / sqrt(d)) V computed by hand equals the module's output."""
    torch.manual_seed(0)
    m = ScaledDotProductSelfAttention(8)
    x = torch.randn(2, 5, 8)
    out, w = m(x)
    q, k, v = m.w_q(x), m.w_k(x), m.w_v(x)
    expected_w = torch.softmax(q @ k.transpose(1, 2) / 8 ** 0.5, dim=-1)
    assert torch.allclose(w, expected_w, atol=1e-6)
    assert torch.allclose(out, expected_w @ v, atol=1e-6)
