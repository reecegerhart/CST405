# Model Card: Forecast-It

## Model details
Two forecasters trained on the same data, with the same preprocessing, optimizer, loss, epoch budget and seed. Only the architecture differs.
- **LSTM baseline:** `LSTM(1 → 32)` over 48 hourly temperatures, forecast from the final hidden state through a linear head.
- **Self-attention model:** a linear embedding of each hour plus a sinusoidal position code, then **one hand-written scaled dot-product self-attention block** (`softmax(QKᵀ/√d)V`, residual + LayerNorm), forecast from the last hour's output row. It does not use `nn.MultiheadAttention`. The 48×48 attention matrix is returned with every prediction.
- **Task:** given the last 48 hourly air temperatures (°C), predict the temperature 24 hours after the last input hour.
- **Training:** Adam, MSE loss on standardized values, gradient clipping at 1.0, the epoch with the best validation MAE is kept. Test MAE is reported in °C.

## Intended use
A campus operations team planning outdoor events needs a rough 24-hour-ahead temperature, together with *which past hours* the model leaned on and an audit trail of every forecast. It is a decision aid for low-stakes scheduling, not a weather service.

**Out of scope:** safety-critical decisions (heat or cold emergencies, evacuations), any location other than central Germany's climate, any horizon other than 24 h.

## Data
- Jena Climate (Max Planck Institute for Biogeochemistry, 2009 to 2016), temperature column `T (degC)`, subsampled to one reading per hour. The most recent 20,000 hours are loaded into Supabase (`sequences`).
- Chronological split, never shuffled across time: 70% train, 15% validation, 15% test. The scaler is fitted on the train split only.

## Metrics
Held-out test MAE (and MSE) in °C, stored per run in Supabase (`runs`) and compared in the README by SQL (`model_comparison` view). Every run also stores the MAE of a **persistence** reference ("tomorrow equals right now"), so a model that does not beat it is easy to spot.

## Uncertainty the product shows (and hides)
- **Shown:** a ±band equal to the 90th percentile of absolute validation error (`resid_q90`), the persistence reference, and the attention weights.
- **Hidden / not captured:** the band is one number for every input, so it does not widen for unusual weather. Attention shows where the model looked, not how likely it is to be right. The model sees only past temperature, with no pressure, humidity, wind or cloud, so it cannot anticipate fronts. Climate drift after 2016 is not represented.

## Privacy: what we log (and don't)
Every prediction stores a **SHA-256 of the input window**, the forecast and the attention vector, plus the run that served it. Raw input readings are never stored. The training corpus is public weather data with no personal information. The fitted model lives in `run_artifacts`, which the anon key cannot read.

## Limitations and risks
- Errors are largest when the weather changes character (fronts, snow cover, fog), which is when a wrong forecast matters most.
- Sharp attention can look like certainty. It isn't. See the Failure Gallery tab.
- Anyone relying on a forecast beyond the stated band is outside the product's evidence.
