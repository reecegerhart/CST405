"""Regression test on a frozen reference input.

`fixtures/reference_artifact.b64` is a frozen attention model (fixed weights, not
trained) and `fixtures/reference.json` holds one input window plus the forecast and
attention row it produced when the fixture was made. If the forward pass changes
(architecture, scaling, positional encoding, de-standardizing), these numbers move
and this test fails. Regenerate ON PURPOSE with `python -m tests.make_reference`.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np

from api.training import predict_windows

FIX = Path(__file__).parent / "fixtures"


def test_frozen_reference_input_gives_frozen_forecast():
    ref = json.loads((FIX / "reference.json").read_text())
    artifact = (FIX / "reference_artifact.b64").read_text()
    out = predict_windows(artifact, [ref["window"]])[0]
    assert abs(out["prediction"] - ref["prediction"]) < 1e-3
    assert np.allclose(out["attention_last_row"], ref["attention_last_row"], atol=1e-4)
    assert out["interval_high"] - out["interval_low"] == 5.0          # 2 x frozen resid_q90 (2.5)
