# hub.py — UI datos: BMP280 (temp/presión/altitud) + KY-025 (mag) + GP2Y1010 (polvo)
from flask import Flask, request, jsonify, Response
import time, json

app = Flask(__name__)

# ===== Últimos valores =====
LATEST = {
    "temp": None,         # °C
    "pressure": None,     # hPa
    "altitude": None,     # m
    "mag_raw": None,      # 0..4095
    "mag_percent": None,  # 0..100 %
    "dust": None,         # µg/m3
    "dust_v": None,       # Volt (opcional)
    "dust_adc": None,     # ADC 0..4095 (opcional)
    "ts": 0,              # unix
}

HTML = r"""<!doctype html><html lang="es">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CENTRO DE MONITOREO</title>
<style>
:root{
  --bg:#1e2227; --panel:#2b323a; --panel2:#23292f; --fg:#e8eaed;
  --muted:#9aa0a6; --ok:#22c55e; --bad:#ef4444; --accent:#ff3b30;
  --cel1:#63d2ff; --cel2:#2fb0ff; --cel3:#16b1f0;
}
*{box-sizing:border-box} html,body{height:100%}
body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,Segoe UI,Roboto,Arial}

.header{
  position:sticky; top:0; z-index:5;
  background:linear-gradient(180deg, rgba(22,27,32,.9), rgba(22,27,32,.65));
  border-bottom:1px solid #33404a;
  padding:18px 20px; margin:0 0 14px 0; box-shadow:0 10px 30px rgba(0,0,0,.25);
}
.title{
  margin:0; line-height:1; font-weight:900; letter-spacing:.12em; text-transform:uppercase;
  font-size: clamp(22px, 4vw, 36px);
  background:linear-gradient(90deg, var(--cel1), var(--cel2) 50%, var(--cel3));
  -webkit-background-clip:text; background-clip:text; color:transparent;
  text-shadow:0 0 14px rgba(55,196,255,.25), 0 0 4px rgba(55,196,255,.35);
}
.subtitle{margin:8px 0 0 0; font-size:12px; color:#9fb6c6; letter-spacing:.18em; text-transform:uppercase}

.app{display:grid; grid-template-columns: repeat(12, 1fr); gap:16px; padding:0 18px 24px}
.full{grid-column: 1/-1}
.card{
  background:linear-gradient(#2b323a,#272e35);
  border:1px solid #3a424a; border-radius:16px; padding:18px;
  box-shadow:inset 0 1px 0 rgba(255,255,255,0.03);
}
.card h3{margin:0 0 10px; font-size:13px; font-weight:700; letter-spacing:.06em; color:#cfd3d7}

.kpi{ display:flex; align-items:baseline; justify-content:space-between; }
.val{ font-size: clamp(36px, 6vw, 56px); font-weight:900; line-height:.9; }
.unit{ margin-left:10px; font-size:14px; color:var(--muted); font-weight:600; }

.pill{ display:inline-flex; align-items:center; gap:8px; padding:7px 12px; border-radius:999px;
  background:#1d2329; border:1px solid #3a424a; color:#cfd3d7; font-size:12px; font-weight:700; }
.pill.ok{ background:#082414; border-color:#1f8f47; color:#a7efbf }
.pill.bad{ background:#2a1414; border-color:#743333; color:#ffc5c5 }

.row{display:flex; flex-wrap:wrap; gap:10px; align-items:center}
.small{font-size:12px; color:var(--muted)}

.col-4{grid-column: span 4}
.col-6{grid-column: span 6}
@media(max-width:1100px){ .col-4{grid-column: span 6} }
@media(max-width:680px){ .col-6,.col-4,.full{grid-column: 1/-1} }

@keyframes bump{0%{transform:scale(1)}30%{transform:scale(1.06)}100%{transform:scale(1)}}
.bump{animation:bump .35s ease}

.badge{display:inline-block; padding:4px 8px; border-radius:8px; background:#1a1f24; border:1px solid #3a424a; color:#cfd3d7; font-size:11px; font-weight:600;}
.bar{ width:100%; height:12px; border-radius:8px; background:#1a1f24; border:1px solid #3a424a; overflow:hidden;}
.bar > div{ height:100%; width:0%; background:linear-gradient(90deg, #1e90ff, #63d2ff); transition: width .25s ease; }
</style>
</head>
<body>

<header class="header">
  <h1 class="title">CENTRO DE MONITOREO</h1>
  <div class="subtitle">ESP32 • BMP280 + KY-025 + GP2Y1010</div>
</header>

<div class="app">
  <div class="card full">
    <div class="row">
      <div id="status" class="pill bad">Sin conexión</div>
      <div class="small">Última actualización: <span id="last">—</span></div>
      <div class="badge" id="age">—</div>
    </div>
  </div>

  <div class="card col-4">
    <h3>TEMPERATURA</h3>
    <div class="kpi">
      <div class="val" id="temp">—</div>
      <div class="unit">°C</div>
    </div>
  </div>

  <div class="card col-4">
    <h3>ATMÓSFERA</h3>
    <div class="kpi">
      <div class="val" id="atm">—</div>
      <div class="unit">hPa</div>
    </div>
  </div>

  <div class="card col-4">
    <h3>ALTITUD</h3>
    <div class="kpi">
      <div class="val" id="alt">—</div>
      <div class="unit">m</div>
    </div>
  </div>

  <div class="card col-6">
    <h3>MAGNETISMO (KY-025)</h3>
    <div class="kpi" style="margin-bottom:8px">
      <div class="val" id="mag">—</div>
      <div class="unit">raw</div>
    </div>
    <div class="row small" style="margin-bottom:8px">
      <div>Intensidad: <span id="magp">—</span> %</div>
    </div>
    <div class="bar"><div id="bar"></div></div>
  </div>

  <div class="card col-6">
    <h3>POLVO (GP2Y1010)</h3>
    <div class="kpi">
      <div class="val" id="dust">—</div>
      <div class="unit">µg/m³</div>
    </div>
    <div class="row small">
      <div>Voltaje: <span id="dv">—</span> V</div>
      <div>| ADC: <span id="da">—</span></div>
    </div>
  </div>

</div>

<script>
const sts  = document.getElementById('status');
const last = document.getElementById('last');
const age  = document.getElementById('age');

const tEl  = document.getElementById('temp');
const pEl  = document.getElementById('atm');
const aEl  = document.getElementById('alt');

const mEl  = document.getElementById('mag');
const mpEl = document.getElementById('magp');
const bar  = document.getElementById('bar');

const dEl  = document.getElementById('dust');
const dvEl = document.getElementById('dv');
const daEl = document.getElementById('da');

let lastOkUnix = 0;
let prev = {t:null,p:null,a:null,m:null,mp:null,d:null};

function setStatus(ok){ if(ok){sts.textContent="Conectado";sts.classList.remove('bad');sts.classList.add('ok');} else {sts.textContent="Sin conexión";sts.classList.remove('ok');sts.classList.add('bad');} }
function bump(el){ el.classList.remove('bump'); void el.offsetWidth; el.classList.add('bump'); }

async function pull(){
  try{
    const r = await fetch('/metrics.json?t='+Date.now(), {cache:'no-store'});
    if(!r.ok) throw new Error(r.status);
    const j = await r.json();
    const nowU = Math.floor(Date.now()/1000);

    last.textContent  = new Date().toLocaleTimeString();
    lastOkUnix        = nowU;

    // BMP280
    if (j.temp!=null){ const v=Number(j.temp); tEl.textContent=v.toFixed(1); if(prev.t==null||Math.abs(v-prev.t)>0.05) bump(tEl); prev.t=v; } else tEl.textContent='—';
    if (j.pressure!=null){ const v=Number(j.pressure); pEl.textContent=v.toFixed(1); if(prev.p==null||Math.abs(v-prev.p)>0.2) bump(pEl); prev.p=v; } else pEl.textContent='—';
    if (j.altitude!=null){ const v=Number(j.altitude); aEl.textContent=v.toFixed(1); if(prev.a==null||Math.abs(v-prev.a)>0.2) bump(aEl); prev.a=v; } else aEl.textContent='—';

    // KY-025
    if (j.mag_raw!=null){ const v=Number(j.mag_raw); mEl.textContent=v.toFixed(0); if(prev.m==null||Math.abs(v-prev.m)>=1) bump(mEl); prev.m=v; } else mEl.textContent='—';
    if (j.mag_percent!=null){ const v=Number(j.mag_percent); mpEl.textContent=v.toFixed(1); if(prev.mp==null||Math.abs(v-prev.mp)>0.5) bump(mpEl); prev.mp=v; bar.style.width = Math.max(0, Math.min(100, v)) + '%'; } else { mpEl.textContent='—'; bar.style.width='0%'; }

    // GP2Y1010
    if (j.dust!=null){ const v=Number(j.dust); dEl.textContent=v.toFixed(1); if(prev.d==null||Math.abs(v-prev.d)>0.5) bump(dEl); prev.d=v; } else dEl.textContent='—';
    dvEl.textContent = (j.dust_v!=null) ? Number(j.dust_v).toFixed(3) : '—';
    daEl.textContent = (j.dust_adc!=null) ? Number(j.dust_adc).toFixed(0) : '—';

  }catch(e){
    // silencio
  }finally{
    const nowU = Math.floor(Date.now()/1000);
    const alive = (nowU - lastOkUnix) <= 5;
    setStatus(alive);
    age.textContent = 'Refresco ~1 s ' + (alive ? '• en vivo' : '• sin datos >5 s');
    setTimeout(pull, 1000);
  }
}
pull();
</script>
</body></html>
"""

# ---------- API ----------
@app.get("/")
def ui_root():
  return Response(HTML, mimetype="text/html")

@app.get("/metrics.json")
def metrics_json():
  return jsonify(LATEST)

@app.post("/ingest")
def ingest():
  try:
    if request.is_json:
      data = request.get_json(force=True)
    else:
      raw = request.data.decode("utf-8", "ignore")
      data = json.loads(raw) if raw else {}
  except Exception as e:
    return jsonify(ok=False, err=f"bad json: {e}"), 400

  for k in ("temp","pressure","altitude","mag_raw","mag_percent","dust","dust_v","dust_adc"):
    if k in data:
      LATEST[k] = data[k]
  LATEST["ts"] = int(time.time())
  return jsonify(ok=True)

@app.get("/health")
def health():
  return "ok", 200

@app.route("/<path:_x>", methods=["GET"])
def catch_all(_x):
  if _x in ("metrics.json","ingest","health"):
    return ("Not Found", 404)
  return Response(HTML, mimetype="text/html")

if __name__ == "__main__":
  print("Servidor: http://127.0.0.1:8000/   (o http://<IP_PC>:8000/)")
  app.run(host="0.0.0.0", port=8000, debug=False, threaded=True)
