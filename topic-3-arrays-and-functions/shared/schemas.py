"""Pydantic request/response models: the contract between the three clouds.

Input validation lives here. A window must be EXACTLY 48 hourly readings, each a
real JSON number (strings and booleans are rejected, not silently coerced), finite,
and inside a physically plausible range for air temperature.
"""
from __future__ import annotations

import math
from datetime import datetime
from typing import Any, Dict, List, Literal, Optional, Union

from pydantic import BaseModel, Field, StrictFloat, StrictInt, field_validator

from shared.data import HORIZON, MAX_TEMP_C, MIN_TEMP_C, WINDOW

Number = Union[StrictInt, StrictFloat]
ModelType = Literal["lstm", "attention"]
MAX_BATCH = 50


def _check_window(w: List[Number]) -> List[Number]:
    for v in w:
        if not math.isfinite(v):
            raise ValueError("window values must be finite numbers")
        if not (MIN_TEMP_C <= v <= MAX_TEMP_C):
            raise ValueError(f"temperatures must be between {MIN_TEMP_C} and {MAX_TEMP_C} degC")
    return w


class PredictRequest(BaseModel):
    run_id: int = Field(..., ge=1)
    window: List[Number] = Field(
        ..., min_length=WINDOW, max_length=WINDOW,
        description=f"The last {WINDOW} hourly temperatures in degC, oldest first.",
    )

    @field_validator("window")
    @classmethod
    def _window_ok(cls, w):
        return _check_window(w)


class PredictBatchRequest(BaseModel):
    run_id: int = Field(..., ge=1)
    windows: List[List[Number]] = Field(..., min_length=1, max_length=MAX_BATCH)

    @field_validator("windows")
    @classmethod
    def _each(cls, ws):
        for w in ws:
            if len(w) != WINDOW:
                raise ValueError(f"every window must have exactly {WINDOW} values")
            _check_window(w)
        return ws


class PredictResponse(BaseModel):
    run_id: int
    model_type: ModelType
    horizon_hours: int = HORIZON
    prediction: float = Field(..., description="Forecast temperature in degC, 24 h after the last input hour.")
    interval_low: float
    interval_high: float
    interval_note: str = "About 90% of validation-set forecasts fell inside this band."
    input_hash: str = Field(..., description="SHA-256 of the input window; raw values are not stored.")
    attention_matrix: Optional[List[List[float]]] = Field(
        None, description="[48 x 48] self-attention weights (rows sum to 1); null for the LSTM."
    )
    attention_last_row: Optional[List[float]] = Field(
        None, description="Which past hours the final position attended to (48 weights)."
    )


class PredictBatchResponse(BaseModel):
    results: List[PredictResponse]


class TrainRequest(BaseModel):
    model_type: ModelType
    lr: float = Field(0.003, gt=0.0, le=1.0)
    batch_size: int = Field(64, ge=8, le=1024)
    epochs: int = Field(12, ge=1, le=100)
    hidden_dim: int = Field(32, ge=8, le=128)
    max_train_rows: Optional[int] = Field(
        None, ge=500, description="Cap rows read per split (keeps the Render free tier fast)."
    )


class Run(BaseModel):
    id: int
    model_type: ModelType
    hyperparameters: Dict[str, Any]
    test_mae: float
    test_mse: float
    val_mae: float
    persistence_mae: float
    resid_q90: float
    n_params: int
    train_seconds: float
    created_at: datetime


class TrainResponse(BaseModel):
    run: Run


class StoredPrediction(BaseModel):
    id: int
    input_hash: str
    predicted_value: float
    attention_vector_json: Optional[List[float]] = None
    served_by_run_id: int
    created_at: datetime


class Health(BaseModel):
    status: str
    model_loader: bool
    supabase: bool


class Version(BaseModel):
    git_sha: str
    torch_version: str
    supabase_project_ref: Optional[str]
