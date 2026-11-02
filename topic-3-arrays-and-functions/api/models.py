"""The two forecasters being compared.

Both take a standardized window ``x [B, T]`` and return ``(prediction [B], attention)``.

* ``LSTMForecaster``      -- recurrent baseline. Reads the window one hour at a time
  and forecasts from the final hidden state. It has no attention, so ``attention``
  is ``None``.
* ``AttentionForecaster`` -- replaces the recurrent cell with ONE hand-written
  scaled dot-product self-attention block (no ``nn.MultiheadAttention``). The
  forecast is read from the last hour's output row, so the last row of the
  attention matrix says which past hours that final position looked at.
"""
from __future__ import annotations

import math
from typing import Optional, Tuple

import torch
from torch import nn


class LSTMForecaster(nn.Module):
    def __init__(self, hidden_dim: int = 32):
        super().__init__()
        self.lstm = nn.LSTM(input_size=1, hidden_size=hidden_dim, batch_first=True)
        self.head = nn.Linear(hidden_dim, 1)

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, Optional[torch.Tensor]]:
        out, _ = self.lstm(x.unsqueeze(-1))              # [B, T, H]
        return self.head(out[:, -1, :]).squeeze(-1), None


class ScaledDotProductSelfAttention(nn.Module):
    r"""Single-head self-attention, written out by hand.

    .. math::  \mathrm{Attn}(X) = \mathrm{softmax}\!\left(\frac{QK^{\top}}{\sqrt{d_k}}\right)V,
               \quad Q = XW_Q,\; K = XW_K,\; V = XW_V
    """

    def __init__(self, d_model: int):
        super().__init__()
        self.d_k = d_model
        self.w_q = nn.Linear(d_model, d_model, bias=False)
        self.w_k = nn.Linear(d_model, d_model, bias=False)
        self.w_v = nn.Linear(d_model, d_model, bias=False)

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
        q, k, v = self.w_q(x), self.w_k(x), self.w_v(x)   # each [B, T, d]
        scores = q @ k.transpose(-2, -1) / math.sqrt(self.d_k)   # [B, T, T]
        weights = torch.softmax(scores, dim=-1)                  # rows sum to 1
        return weights @ v, weights                              # [B, T, d], [B, T, T]


def sinusoidal_positions(length: int, d_model: int) -> torch.Tensor:
    """Fixed sin/cos positional encoding (attention alone has no notion of order)."""
    pos = torch.arange(length, dtype=torch.float32).unsqueeze(1)
    div = torch.exp(torch.arange(0, d_model, 2, dtype=torch.float32) * (-math.log(10000.0) / d_model))
    pe = torch.zeros(length, d_model)
    pe[:, 0::2] = torch.sin(pos * div)
    pe[:, 1::2] = torch.cos(pos * div[: pe[:, 1::2].shape[1]])
    return pe


class AttentionForecaster(nn.Module):
    def __init__(self, hidden_dim: int = 32, window: int = 48):
        super().__init__()
        self.embed = nn.Linear(1, hidden_dim)
        self.register_buffer("pos", sinusoidal_positions(window, hidden_dim), persistent=False)
        self.attn = ScaledDotProductSelfAttention(hidden_dim)
        self.norm = nn.LayerNorm(hidden_dim)
        self.head = nn.Linear(hidden_dim, 1)

    def forward(self, x: torch.Tensor) -> Tuple[torch.Tensor, torch.Tensor]:
        h = self.embed(x.unsqueeze(-1)) + self.pos[: x.shape[1]]   # [B, T, d]
        ctx, weights = self.attn(h)
        h = self.norm(h + ctx)                                     # residual + norm
        return self.head(h[:, -1, :]).squeeze(-1), weights         # forecast from last hour


MODEL_TYPES = ("lstm", "attention")


def build_model(model_type: str, hidden_dim: int = 32, window: int = 48) -> nn.Module:
    if model_type == "lstm":
        return LSTMForecaster(hidden_dim)
    if model_type == "attention":
        return AttentionForecaster(hidden_dim, window)
    raise ValueError(f"model_type must be one of {MODEL_TYPES}, got {model_type!r}")
