"""Print the baseline-vs-attention comparison as a Markdown table, straight from the
`model_comparison` SQL view (see db/migrations/001_init.sql), ready to paste in the README.

    python -m db.readme_table

The equivalent SQL, if you prefer to run it in the Supabase SQL Editor:

    select model_type, run_id, test_mae, persistence_mae, n_params, train_seconds
    from model_comparison order by test_mae;
"""
from __future__ import annotations

from dotenv import load_dotenv

from api import db


def main() -> None:
    load_dotenv()
    rows = (
        db.get_client()
        .table("model_comparison")
        .select("model_type,run_id,test_mae,test_mse,persistence_mae,n_params,train_seconds")
        .order("test_mae")
        .execute()
        .data
    )
    print("| Model | Run | Test MAE (degC) | Test MSE | Persistence MAE | Params | Train time (s) |")
    print("|---|---|---|---|---|---|---|")
    for r in rows:
        print(f"| {r['model_type']} | {r['run_id']} | {r['test_mae']:.3f} | {r['test_mse']:.3f} | "
              f"{r['persistence_mae']:.3f} | {r['n_params']:,} | {r['train_seconds']:.0f} |")


if __name__ == "__main__":
    main()
