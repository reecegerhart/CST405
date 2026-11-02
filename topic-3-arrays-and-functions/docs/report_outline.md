# Engineering report outline (750 to 1000 words, your own words)

The assignment requires each team member to write their own report, so this is a scaffold, not a draft. Fill in the numbers from your own `runs` table.

## 1. Engineering analysis (two or more concrete observations, each tied to structure)
Compare the two runs (`python -m db.readme_table`, Baseline vs. Attention tab). For each observation say what you saw, then WHY the architecture explains it. Candidates, to confirm against your numbers before using them:
- **Accuracy vs. persistence.** Is each model better than "tomorrow = right now"? A 24 h horizon is one full daily cycle, so persistence is a strong baseline on hourly temperature.
- **Training time and parameter count** (`train_seconds`, `n_params`). The LSTM must process 48 steps sequentially; attention computes all pairs in one matrix multiply but is O(T²).
- **Where the attention looked** (Try It heatmap, `attention_peak_hours_ago` in the examples). Does the last row peak near 24 or 48 hours back (the same time of day)? That is a structural property: any hour can attend directly to any other, with no 47-step path.
- **Interpretability.** The LSTM has no weights to show; attention returns a matrix (but see the Failure Gallery: sharp attention is not the same as correctness).
- **Position.** Attention has no sense of order without the positional encoding. What happens if you remove it? (A good experiment to mention if you try it.)

## 2. Risk and ethics reflection (Variant A)
- Who is harmed if the forecast is confidently wrong? (event attendees in unexpected heat or cold, vulnerable people, grounds and facilities staff)
- Guardrails: the error band (`resid_q90`) and its limits (same width for every input), the persistence fallback, a climatology fallback, refusing out-of-range input, a human sign-off for safety-relevant calls.
- What is and is not persisted: only the SHA-256 of the window, the forecast and the attention vector. Why not the raw readings?

## 3. Worldview reflection
Proverbs 27:1: "Do not boast about tomorrow, for you do not know what a day may bring." State the uncertainty the product **shows** (error band, persistence reference, attention) and the uncertainty it **hides** (the band is constant, attention is not a probability, only past temperature is used, climate drift since 2016).
