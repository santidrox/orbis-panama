from flask import Flask, request, jsonify, send_from_directory
from pathlib import Path
from time import time, strftime, localtime

app = Flask(__name__)
BASE_DIR = Path(__file__).resolve().parent
HTML_NAME = "hud_local.html"

# Estado en memoria
metrics = {
    "temp": None,
    "humidity": None,
    "dust": None,
    "gravity": 9.81,
    "atm": 1013,
    "last_update": None
}

@app.post("/metrics")
def update_metrics():
    data = request.get_json(silent=True) or {}
    # Solo actualiza campos presentes
    for k in ["temp", "humidity", "dust", "gravity", "atm"]:
        if k in data:
            metrics[k] = data[k]
    metrics["last_update"] = time()
    print("📥", request.remote_addr, "->", data)
    return jsonify(success=True)

@app.get("/metrics.json")
def get_metrics():
    out = dict(metrics)
    if out["last_update"] is not None:
        out["last_update_human"] = strftime("%Y-%m-%d %H:%M:%S", localtime(out["last_update"]))
    return jsonify(out)

@app.get("/")
def index():
    return send_from_directory(str(BASE_DIR), HTML_NAME)

@app.get("/favicon.ico")
def fav():
    return ("", 204)

if __name__ == "__main__":
    # Escucha en todas las interfaces para que la ESP pueda pegarle por IP local
    app.run(host="0.0.0.0", port=5000)
