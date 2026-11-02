"""Training, evaluation and inference for the two forecasters.

This is the ONLY place model code is used by the service. Both architectures go
through the SAME function with the SAME preprocessing, optimizer, loss, epoch
budget and seed, so the comparison in the README is like-for-like:

    standardize (train-split mean/std) -> 48 h windows -> Adam + MSE -> keep the
    epoch with the best validation loss -> report test MAE in degrees C.

The fitted model, its scaler and a calibrated error band are packed into one
base64 string so they can be stored in Supabase (`run_artifacts`) and reloaded on
every request -- the Render free tier restarts often and has no persistent disk.
"""
from __future__ import annotations

import base64
import io
import time
from typing import Dict, List, Optional

import numpy as np
import torch
from torch import nn

from api.models import build_model
from shared.data import HORIZON, WINDOW, Scaler, make_windows

DEFAULT_HP = {"lr": 0.003, "batch_size": 64, "epochs": 12, "hidden_dim": 32, "seed": 0}


def _to_tensor(a: np.ndarray) -> torch.Tensor:
    return torch.from_numpy(np.ascontiguousarray(a, dtype=np.float32))


def _predict_scaled(model: nn.Module, x: torch.Tensor, batch: int = 1024):
    """Run the model in batches; return (preds [N], last attention rows [N,T] | None)."""
    model.eval()
    preds, rows = [], []
    with torch.no_grad():
        for s in range(0, len(x), batch):
            p, w = model(x[s : s + batch])
            preds.append(p)
            if w is not None:
                rows.append(w[:, -1, :])
    return torch.cat(preds).numpy(), (torch.cat(rows).numpy() if rows else None)


def _entropy(rows: np.ndarray) -> np.ndarray:
    return -(rows * np.log(rows + 1e-12)).sum(axis=1)


def train_model(
    train: np.ndarray,
    val: np.ndarray,
    test: np.ndarray,
    model_type: str,
    lr: float = DEFAULT_HP["lr"],
    batch_size: int = DEFAULT_HP["batch_size"],
    epochs: int = DEFAULT_HP["epochs"],
    hidden_dim: int = DEFAULT_HP["hidden_dim"],
    seed: int = DEFAULT_HP["seed"],
) -> Dict[str, object]:
    """Fit one model on hourly-temperature splits (1-D arrays, in time order)."""
    started = time.time()
    scaler = Scaler.fit(train)
    xs, ys = {}, {}
    for name, series in (("train", train), ("val", val), ("test", test)):
        x, y = make_windows(series)
        xs[name], ys[name] = x, y            # raw degC, kept for metrics/examples
    x_train, y_train = _to_tensor(scaler.transform(xs["train"])), _to_tensor(scaler.transform(ys["train"]))
    x_val, y_val = _to_tensor(scaler.transform(xs["val"])), ys["val"]
    x_test = _to_tensor(scaler.transform(xs["test"]))

    torch.manual_seed(seed)
    model = build_model(model_type, hidden_dim=hidden_dim, window=WINDOW)
    optimizer = torch.optim.Adam(model.parameters(), lr=lr)
    loss_fn = nn.MSELoss()

    g = torch.Generator().manual_seed(seed)
    n = len(x_train)
    best_val, best_state, loss_history = float("inf"), None, []
    for _ in range(epochs):
        model.train()
        perm = torch.randperm(n, generator=g)
        total = 0.0
        for s in range(0, n, batch_size):
            b = perm[s : s + batch_size]
            optimizer.zero_grad()
            pred, _ = model(x_train[b])
            loss = loss_fn(pred, y_train[b])
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), 1.0)
            optimizer.step()
            total += loss.item() * len(b)
        loss_history.append(total / n)
        v_pred, _ = _predict_scaled(model, x_val)
        v_mae = float(np.abs(scaler.inverse(v_pred) - y_val).mean())
        if v_mae < best_val:
            best_val = v_mae
            best_state = {k: t.clone() for k, t in model.state_dict().items()}
    model.load_state_dict(best_state)

    # ---- evaluation (degrees C) -------------------------------------------------
    v_pred = scaler.inverse(_predict_scaled(model, x_val)[0])
    resid_q90 = float(np.quantile(np.abs(v_pred - y_val), 0.90))   # calibrated on VAL
    t_scaled, t_rows = _predict_scaled(model, x_test)
    t_pred, t_true = scaler.inverse(t_scaled), ys["test"]
    err = t_pred - t_true
    metrics = {
        "test_mae": float(np.abs(err).mean()),
        "test_mse": float((err ** 2).mean()),
        "val_mae": float(best_val),
        "persistence_mae": float(np.abs(xs["test"][:, -1] - t_true).mean()),
        "resid_q90": resid_q90,
        "n_params": int(sum(p.numel() for p in model.parameters())),
        "train_seconds": float(time.time() - started),
        "n_train_windows": int(n),
    }

    examples = _build_examples(xs["test"], t_true, t_pred, t_rows, err)

    buf = io.BytesIO()
    torch.save(
        {
            "state_dict": model.state_dict(),
            "model_type": model_type,
            "hidden_dim": hidden_dim,
            "window": WINDOW,
            "horizon": HORIZON,
            "scaler": scaler.to_dict(),
            "resid_q90": resid_q90,
        },
        buf,
    )
    return {
        "metrics": metrics,
        "model_b64": base64.b64encode(buf.getvalue()).decode("ascii"),
        "loss_history": loss_history,
        "examples": examples,
        "test_predictions": t_pred,   # same test windows for both models (used by the CLI)
    }


def _build_examples(x_raw, y_true, y_pred, rows: Optional[np.ndarray], err) -> Dict[str, list]:
    """Three illustrative test windows, plus (attention model only) three failures.

    'Confidently wrong' for a regressor = large error while the last attention row is
    sharply peaked (entropy in the lowest 20% of the test set).
    """

    def item(i: int) -> dict:
        d = {
            "test_index": int(i),
            "window": [round(float(v), 2) for v in x_raw[i]],
            "target": round(float(y_true[i]), 2),
            "prediction": round(float(y_pred[i]), 2),
            "abs_error": round(float(abs(err[i])), 2),
        }
        if rows is not None:
            d["attention"] = [round(float(v), 5) for v in rows[i]]
            d["attention_peak_hours_ago"] = int(WINDOW - 1 - np.argmax(rows[i]))
        return d

    n = len(y_true)
    out: Dict[str, list] = {"illustrative": [item(int(n * f)) for f in (0.10, 0.50, 0.90)]}
    if rows is not None:
        ent = _entropy(rows)
        sharp = np.where(ent <= np.quantile(ent, 0.20))[0]
        worst = sharp[np.argsort(-np.abs(err[sharp]))[:3]]
        out["failures"] = [dict(item(int(i)), attention_entropy=round(float(ent[i]), 3)) for i in worst]
    return out


# ---------------------------------------------------------------------------
# inference
# ---------------------------------------------------------------------------
def load_artifact(model_b64: str):
    obj = torch.load(io.BytesIO(base64.b64decode(model_b64)), map_location="cpu", weights_only=True)
    model = build_model(obj["model_type"], hidden_dim=obj["hidden_dim"], window=obj["window"])
    model.load_state_dict(obj["state_dict"])
    model.eval()
    return model, Scaler.from_dict(obj["scaler"]), obj


def predict_windows(model_b64: str, windows: List[List[float]]) -> List[Dict[str, object]]:
    """Forecast each 48-hour window; attention (if the model has any) comes back too."""
    model, scaler, meta = load_artifact(model_b64)
    x = _to_tensor(scaler.transform(np.asarray(windows, dtype=np.float32)))
    with torch.no_grad():
        pred, weights = model(x)
    values = scaler.inverse(pred.numpy())
    q = float(meta["resid_q90"])
    results = []
    for i, v in enumerate(values):
        w = weights[i].tolist() if weights is not None else None
        results.append(
            {
                "model_type": meta["model_type"],
                "prediction": float(v),
                "interval_low": float(v) - q,
                "interval_high": float(v) + q,
                "attention_matrix": w,                                  # [T, T] or None
                "attention_last_row": w[-1] if w is not None else None,  # [T] or None
            }
        )
    return results
