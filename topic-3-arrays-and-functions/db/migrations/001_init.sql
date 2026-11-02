-- 001_init.sql  --  Variant A: 24-hour temperature forecasting (LSTM vs. attention)
-- Apply in the Supabase dashboard: SQL Editor -> New query -> paste -> Run.

-- ---------------------------------------------------------------------------
-- sequences: the training corpus (Jena Climate, one temperature per hour).
-- `split` is assigned chronologically by db/load.py (70% train / 15% val / 15% test).
-- ---------------------------------------------------------------------------
create table if not exists sequences (
    id        bigint generated always as identity primary key,
    ts        timestamptz      not null,
    value     double precision not null,      -- temperature, degrees C
    split     text             not null check (split in ('train', 'val', 'test'))
);
create index if not exists idx_sequences_split_ts on sequences (split, ts);

-- ---------------------------------------------------------------------------
-- runs: one row per trained model (lstm vs. attention) with its headline metric.
-- `examples` holds the illustrative and "confidently wrong" test windows so the UI
-- can show them without re-running anything.
-- ---------------------------------------------------------------------------
create table if not exists runs (
    id               bigint generated always as identity primary key,
    model_type       text   not null check (model_type in ('lstm', 'attention')),
    hyperparameters  jsonb  not null,
    test_mae         double precision not null,   -- headline metric, degrees C
    test_mse         double precision not null,
    val_mae          double precision not null,
    persistence_mae  double precision not null,   -- "tomorrow = right now" reference
    resid_q90        double precision not null,   -- 90% error band half-width, degrees C
    n_params         integer not null,
    train_seconds    double precision not null,
    examples         jsonb,
    created_at       timestamptz not null default now()
);
create index if not exists idx_runs_created_at on runs (created_at desc);

-- The fitted model + scaler, base64. Not anon-readable.
create table if not exists run_artifacts (
    run_id     bigint primary key references runs (id) on delete cascade,
    model_b64  text not null,
    created_at timestamptz not null default now()
);

-- ---------------------------------------------------------------------------
-- predictions: one row per served prediction (audit trail for the weekly review).
-- Only a SHA-256 of the input window is stored, never the raw readings.
-- ---------------------------------------------------------------------------
create table if not exists predictions (
    id                    bigint generated always as identity primary key,
    input_hash            text   not null,
    predicted_value       double precision not null,   -- degrees C, 24 h ahead
    attention_vector_json jsonb,                       -- last attention row (48 weights); null for lstm
    served_by_run_id      bigint not null references runs (id) on delete cascade,
    created_at            timestamptz not null default now()
);
create index if not exists idx_predictions_created_at on predictions (created_at desc);
create index if not exists idx_predictions_run_id     on predictions (served_by_run_id);

-- ---------------------------------------------------------------------------
-- model_comparison: the README table is generated from this view (db/readme_table.py).
-- Latest run of each model type.
-- ---------------------------------------------------------------------------
create or replace view model_comparison as
select distinct on (model_type)
    model_type, id as run_id, test_mae, test_mse, val_mae, persistence_mae,
    n_params, train_seconds, created_at
from runs
order by model_type, created_at desc;

-- ---------------------------------------------------------------------------
-- Row Level Security. The Streamlit UI uses the ANON key and may only SELECT
-- sequences, runs, predictions and the view. Writes use the service-role key.
-- ---------------------------------------------------------------------------
alter table sequences     enable row level security;
alter table runs          enable row level security;
alter table run_artifacts enable row level security;
alter table predictions   enable row level security;

drop policy if exists "anon can read sequences"   on sequences;
drop policy if exists "anon can read runs"        on runs;
drop policy if exists "anon can read predictions" on predictions;
create policy "anon can read sequences"   on sequences   for select to anon using (true);
create policy "anon can read runs"        on runs        for select to anon using (true);
create policy "anon can read predictions" on predictions for select to anon using (true);

grant select on model_comparison to anon;
