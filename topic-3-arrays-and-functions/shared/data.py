"""Data helpers for Variant A: 24-hour-ahead temperature forecasting (Jena Climate).

The corpus is the Jena Climate temperature column (``T (degC)``), subsampled to one
reading per hour and stored in Supabase (`sequences` table). Everything the model
needs from that corpus lives here so the API, the loader and the tests all build
windows the same way:

    input  : the last WINDOW (48) hourly temperatures
    target : the temperature HORIZON (24) hours after the last input hour

``make_synthetic_series`` produces a Jena-like stand-in (daily + yearly cycle plus
noise) so the tests and local demos run without downloading anything.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Tuple

import numpy as np

WINDOW = 48          # hours of history fed to the model
HORIZON = 24         # hours ahead we forecast
MIN_TEMP_C = -60.0   # validation bounds (Jena's real range is about -23 .. 37)
MAX_TEMP_C = 60.0

SPLITS = ("train", "val", "test")
SPLIT_FRACTIONS = (0.70, 0.15, 0.15)   # chronological, no shuffling across time


@dataclass
class Scaler:
    """Standardizer fitted on the TRAIN split only (no leakage)."""

    mean: float
    std: float

    @classmethod
    def fit(cls, values: np.ndarray) -> "Scaler":
        return cls(mean=float(np.mean(values)), std=float(np.std(values) or 1.0))

    def transform(self, x):
        return (np.asarray(x, dtype=np.float32) - self.mean) / self.std

    def inverse(self, z):
        return np.asarray(z, dtype=np.float32) * self.std + self.mean

    def to_dict(self) -> Dict[str, float]:
        return {"mean": self.mean, "std": self.std}

    @classmethod
    def from_dict(cls, d: Dict[str, float]) -> "Scaler":
        return cls(mean=float(d["mean"]), std=float(d["std"]))


def chronological_split_labels(n: int) -> np.ndarray:
    """Label each of ``n`` time-ordered rows 'train' / 'val' / 'test'."""
    n_train = int(n * SPLIT_FRACTIONS[0])
    n_val = int(n * SPLIT_FRACTIONS[1])
    labels = np.empty(n, dtype=object)
    labels[:n_train] = "train"
    labels[n_train : n_train + n_val] = "val"
    labels[n_train + n_val :] = "test"
    return labels


def make_windows(
    series: np.ndarray, window: int = WINDOW, horizon: int = HORIZON
) -> Tuple[np.ndarray, np.ndarray]:
    """Slide over ``series``; return ``X [N, window]`` and ``y [N]``.

    ``y[i] = series[i + window - 1 + horizon]`` -- the value ``horizon`` hours after
    the last hour in ``X[i]``. Windows never span two splits because the caller
    passes one split's values at a time.
    """
    series = np.asarray(series, dtype=np.float32)
    n = len(series) - window - horizon + 1
    if n <= 0:
        raise ValueError(
            f"Series of length {len(series)} is too short for window={window}, horizon={horizon}."
        )
    idx = np.arange(window)[None, :] + np.arange(n)[:, None]
    return series[idx], series[window - 1 + horizon : window - 1 + horizon + n]


def make_synthetic_series(n_hours: int = 6000, seed: int = 0) -> np.ndarray:
    """Jena-like hourly temperatures: yearly + daily cycle + AR(1) weather noise."""
    rng = np.random.default_rng(seed)
    t = np.arange(n_hours, dtype=np.float64)
    yearly = 9.5 * np.sin(2 * np.pi * (t / (24 * 365.25) - 0.28))
    daily = 4.0 * np.sin(2 * np.pi * (t / 24.0 - 0.38))
    noise = np.zeros(n_hours)
    for i in range(1, n_hours):
        noise[i] = 0.97 * noise[i - 1] + rng.normal(0, 0.6)
    return (9.0 + yearly + daily + noise).astype(np.float32)
