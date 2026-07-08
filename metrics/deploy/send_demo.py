"""读取 demo JSONL → 包装成 PostHog capture 格式 → POST 到 mock 接收器"""
import json, sys, urllib.request, os

host = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8000"
jsonl = sys.argv[2] if len(sys.argv) > 2 else "demo-data/telemetry.jsonl"

with open(jsonl) as f:
    lines = [l.strip() for l in f if l.strip()]

batch = []
for line in lines:
    e = json.loads(line)
    uid = (e["ctx"].get("user_id") or "").strip()
    did = uid if uid else e["ctx"].get("install_id", "")
    batch.append({
        "event": e["event"],
        "properties": e["props"],
        "timestamp": e["ts"],
        "distinct_id": did
    })

payload = json.dumps({"api_key": "demo-key-from-local-mock", "batch": batch}).encode()
url = f"{host}/i/v0/e/"
req = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
resp = urllib.request.urlopen(req)

print(f"POST {url} -> HTTP {resp.status}")
print(f"Sent {len(batch)} events, body {len(payload)} bytes")
print(f"Response: {resp.read().decode()}")
