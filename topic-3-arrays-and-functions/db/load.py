"""Load the Jena Climate temperature corpus into Supabase (`sequences` table).

Run from the repo root, after applying db/migrations/001_init.sql and creating .env:

    python -m db.load                    # download Jena, keep the most recent 20,000 hours
    python -m db.load --hours 40000      # keep more history
    python -m db.load --csv jena.csv     # use a CSV you already downloaded
    python -m db.load --synthetic        # Jena-like fake data (no download; for smoke tests)

Rows are labelled chronologically: 70% train / 15% val / 15% test. Re-running wipes and
reloads the table so the splits stay consistent.
"""
from __future__ import annotations

import argparse
import io
import os
import urllib.request
import zipfile

import numpy as np
import pandas as pd
from dotenv import load_dotenv

from api import db
from shared.data import chronological_split_labels, make_synthetic_series

JENA_URL = "https://storage.googleapis.com/tensorflow/tf-keras-datasets/jena_climate_2009_2016.csv.zip"
BATCH = 1000


def read_jena(csv_path: str | None) -> pd.DataFrame:
    """Return a DataFrame(ts, value) with ONE temperature per hour, in time order."""
    if csv_path:
        raw = pd.read_csv(csv_path)
    else:
        print(f"Downloading {JENA_URL} ...")
        with urllib.request.urlopen(JENA_URL, timeout=120) as r:
            zf = zipfile.ZipFile(io.BytesIO(r.read()))
        raw = pd.read_csv(zf.open(zf.namelist()[0]))
    ts = pd.to_datetime(raw["Date Time"], format="%d.%m.%Y %H:%M:%S")
    df = pd.DataFrame({"ts": ts, "value": raw["T (degC)"].astype(float)})
    df = df[df["ts"].dt.minute == 0].drop_duplicates("ts").sort_values("ts")   # 10-min -> hourly
    return df.reset_index(drop=True)


def main() -> None:
    load_dotenv()
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", help="path to an already-downloaded jena_climate_2009_2016.csv")
    ap.add_argument("--hours", type=int, default=20000, help="keep the most recent N hours")
    ap.add_argument("--synthetic", action="store_true", help="use a synthetic Jena-like series")
    args = ap.parse_args()
    if not (os.environ.get("SUPABASE_URL") and os.environ.get("SUPABASE_SERVICE_KEY")):
        raise SystemExit("Set SUPABASE_URL and SUPABASE_SERVICE_KEY in .env first.")

    if args.synthetic:
        n = min(args.hours, 8000)
        df = pd.DataFrame(
            {"ts": pd.date_range("2015-01-01", periods=n, freq="h"), "value": make_synthetic_series(n)}
        )
    else:
        df = read_jena(args.csv)
    df = df.tail(args.hours).reset_index(drop=True)
    df["split"] = chronological_split_labels(len(df))
    print(df.groupby("split").size().to_string())

    client = db.get_client()
    client.table("sequences").delete().gte("id", 0).execute()      # clean reload
    records = [
        {"ts": t.isoformat(), "value": float(v), "split": s}
        for t, v, s in zip(df["ts"], df["value"], df["split"])
    ]
    for i in range(0, len(records), BATCH):
        client.table("sequences").insert(records[i : i + BATCH]).execute()
        print(f"  inserted {min(i + BATCH, len(records))}/{len(records)}", end="\r")
    print(f"\nLoaded {len(records)} hourly rows into `sequences`.")


if __name__ == "__main__":
    main()
