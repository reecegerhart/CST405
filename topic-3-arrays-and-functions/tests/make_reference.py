"""One-off: regenerate the frozen regression fixture. Only run this on purpose,
e.g. after an intentional change to the model architecture:

    python -m tests.make_reference
"""
import base64, io, json
from pathlib import Path

import torch

from api.models import build_model
from api.training import predict_windows
from shared.data import WINDOW, HORIZON, Scaler, make_synthetic_series

OUT = Path(__file__).parent / "fixtures"


def main():
    torch.manual_seed(1234)
    model = build_model("attention", hidden_dim=16, window=WINDOW)
    buf = io.BytesIO()
    torch.save({"state_dict": model.state_dict(), "model_type": "attention", "hidden_dim": 16, "window": WINDOW,
                "horizon": HORIZON, "scaler": Scaler(9.0, 8.0).to_dict(), "resid_q90": 2.5}, buf)
    b64 = base64.b64encode(buf.getvalue()).decode()
    window = [round(float(v), 2) for v in make_synthetic_series(200, seed=7)[20:20 + WINDOW]]
    res = predict_windows(b64, [window])[0]
    (OUT / "reference_artifact.b64").write_text(b64)
    (OUT / "reference.json").write_text(json.dumps({
        "window": window, "prediction": res["prediction"],
        "attention_last_row": res["attention_last_row"]}, indent=1))
    print("frozen prediction:", res["prediction"])

main()
