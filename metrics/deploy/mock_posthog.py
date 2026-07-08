"""极简 PostHog capture 协议接收器，本地端到端验证用。
监听 POST /i/v0/e/ ，打印接收到的每条事件。
"""
import json, datetime, sys
from flask import Flask, request, jsonify

app = Flask(__name__)
count = {"received": 0}

@app.route("/i/v0/e/", methods=["POST"])
def capture():
    body = request.get_json(force=True) if request.get_data() else {}
    batch = body.get("batch", [])
    api_key = body.get("api_key", "<none>")
    if batch:
        print(f"\n{'='*60}")
        print(f"  PostHog capture batch | key={api_key} | {len(batch)} events")
        for e in batch:
            print(f"  [{e.get('timestamp','')}] {e.get('event','?')} "
                  f"  distinct_id={e.get('distinct_id','?')} "
                  f"  props={json.dumps(e.get('properties',{}))}")
        count["received"] += len(batch)
        print(f"  total received: {count['received']}")
        print(f"{'='*60}\n")
    return jsonify({"status": "ok"}), 200

@app.route("/health", methods=["GET"])
def health():
    return jsonify({"status": "healthy", "received": count["received"]})

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    print(f"PostHog mock receiver listening on :{port}")
    app.run(host="0.0.0.0", port=port)
