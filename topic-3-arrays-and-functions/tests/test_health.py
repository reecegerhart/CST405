def test_healthz_and_version(client):
    h = client.get("/healthz").json()
    assert h == {"status": "ok", "model_loader": True, "supabase": True}
    v = client.get("/version").json()
    assert {"git_sha", "torch_version", "supabase_project_ref"} <= set(v)


def test_runs_lists_trained_models(client, trained):
    runs = client.get("/runs").json()
    assert {r["model_type"] for r in runs} == {"lstm", "attention"}
    assert all(r["test_mae"] > 0 for r in runs)
