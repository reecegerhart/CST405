"""Train BOTH models under the same budget and persist both runs to Supabase.

    python -m api.train_cli                    # defaults: 12 epochs, hidden 32, lr 0.003
    python -m api.train_cli --epochs 20 --max-rows 8000

Same function, data, scaler, optimizer, loss, epochs and seed for each model; the only
thing that changes is the architecture. Reads rows from Supabase and writes the `runs`
rows (the README comparison table is then generated from them with SQL).
"""
from __future__ import annotations

import argparse

from dotenv import load_dotenv

from api import db
from api.models import MODEL_TYPES
from api.training import DEFAULT_HP, train_model


def main() -> None:
    load_dotenv()
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=DEFAULT_HP["epochs"])
    ap.add_argument("--lr", type=float, default=DEFAULT_HP["lr"])
    ap.add_argument("--batch-size", type=int, default=DEFAULT_HP["batch_size"])
    ap.add_argument("--hidden-dim", type=int, default=DEFAULT_HP["hidden_dim"])
    ap.add_argument("--max-rows", type=int, default=None, help="cap rows read per split")
    a = ap.parse_args()

    print("Reading corpus from Supabase ...")
    splits = {s: db.fetch_split(s, a.max_rows) for s in ("train", "val", "test")}
    print({k: len(v) for k, v in splits.items()})

    hp = {"lr": a.lr, "batch_size": a.batch_size, "epochs": a.epochs, "hidden_dim": a.hidden_dim}
    for model_type in MODEL_TYPES:
        out = train_model(splits["train"], splits["val"], splits["test"], model_type, **hp)
        run = db.insert_run(model_type, hp, out["metrics"], out["examples"], out["model_b64"])
        m = out["metrics"]
        print(f"[{model_type:9s}] run_id={run['id']}  test MAE={m['test_mae']:.3f} degC  "
              f"(persistence {m['persistence_mae']:.3f})  params={m['n_params']}  {m['train_seconds']:.0f}s")


if __name__ == "__main__":
    main()
