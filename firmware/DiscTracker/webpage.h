// Single-page UI served to the phone. Everything updates in place - no
// scrolling log - so it stays readable while you're walking.
#pragma once

const char PAGE_HTML[] PROGMEM = R"HTMLPAGE(
<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<meta name="theme-color" content="#0b0f0e">
<title>Disc Tracker</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;padding:10px;background:#0b0f0e;color:#e6f3ea;
 font:16px/1.3 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;
 max-width:640px;margin:0 auto}
.row{display:flex;gap:8px;align-items:center}
.top{justify-content:space-between;margin-bottom:10px}
.pill{padding:6px 14px;border-radius:999px;font-weight:700;font-size:14px;letter-spacing:.5px}
.on{background:#16a34a;color:#fff}.off{background:#374151;color:#9ca3af}
.meta{font-size:13px;color:#9ca3af}
.card{background:#141a18;border:1px solid #223028;border-radius:14px;padding:14px;margin-bottom:10px}
.epc{font-family:ui-monospace,Menlo,monospace;font-size:13px;color:#7dd3a8;
 word-break:break-all;margin-bottom:6px;min-height:17px}
.big{font-size:76px;font-weight:800;line-height:1;letter-spacing:-2px;text-align:center}
.big small{font-size:24px;font-weight:600;color:#6b7280;letter-spacing:0}
.barwrap{height:22px;background:#0b0f0e;border-radius:11px;overflow:hidden;margin:12px 0}
.bar{height:100%;width:0;border-radius:11px;transition:width .15s linear,background .3s}
.stats{display:flex;justify-content:space-around;text-align:center;margin-top:6px}
.stats b{display:block;font-size:22px;font-weight:700}
.stats span{font-size:11px;color:#6b7280;text-transform:uppercase;letter-spacing:.5px}
button{font:inherit;border:none;border-radius:12px;padding:14px;font-weight:700;
 background:#223028;color:#e6f3ea}
button:active{opacity:.7}
.scan{width:100%;font-size:22px;padding:20px;margin-bottom:10px}
.scan.on{background:#dc2626;color:#fff}.scan.off{background:#16a34a;color:#fff}
.half{flex:1;font-size:14px;padding:12px}
.half.act{background:#16a34a;color:#fff}
label{font-size:14px;color:#9ca3af;display:block;margin-bottom:8px}
label b{color:#e6f3ea;font-size:19px}
input[type=range]{width:100%;height:38px;accent-color:#4ade80}
.tag{display:flex;align-items:center;gap:10px;padding:11px 12px;border-radius:11px;
 background:#141a18;border:1px solid #223028;margin-bottom:7px}
.tag.sel{border-color:#4ade80;background:#16241c}
.tag .id{font-family:ui-monospace,Menlo,monospace;font-size:12px;flex:1;
 overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.tag .mini{width:64px;height:7px;background:#0b0f0e;border-radius:4px;overflow:hidden}
.tag .mini i{display:block;height:100%;background:#4ade80}
.tag .db{font-size:12px;color:#9ca3af;width:52px;text-align:right}
.empty{text-align:center;color:#6b7280;padding:22px;font-size:14px}
.lost{color:#ef4444 !important}
</style></head><body>

<div class="row top">
  <span id="status" class="pill off">IDLE</span>
  <span id="meta" class="meta">connecting…</span>
</div>

<div class="card">
  <div id="fepc" class="epc">— tap a tag below to track it —</div>
  <div id="fprox" class="big">0<small>%</small></div>
  <div class="barwrap"><div id="fbar" class="bar"></div></div>
  <div class="stats">
    <div><b id="frssi">—</b><span>dBm</span></div>
    <div><b id="frate">—</b><span>reads/s</span></div>
    <div><b id="freads">—</b><span>total</span></div>
  </div>
</div>

<button id="btnScan" class="scan off">START</button>

<div class="card">
  <label>Signal strength <b id="pwrVal">20</b> dBm
    <span class="meta">— lower = shorter range = sharper gradient</span></label>
  <input type="range" id="pwr" min="5" max="26" value="20">
  <div class="row" style="margin-top:10px">
    <button id="btnAudio" class="half">CLICKS: OFF</button>
    <button id="btnClear" class="half">CLEAR LIST</button>
  </div>
</div>

<div id="list"></div>

<script>
let S={scan:0,pwr:20,t:[]}, focusEpc=null, audioOn=false, actx=null, nextTick=0, busy=false;

const $=id=>document.getElementById(id);
const colour=p=>p>.66?'#4ade80':p>.33?'#facc15':'#ef4444';

async function api(path){ try{ await fetch(path); }catch(e){} }

async function poll(){
  if(busy) return; busy=true;
  try{
    const r=await fetch('/api/state',{cache:'no-store'});
    S=await r.json(); render(); $('meta').textContent=S.n+' tag'+(S.n==1?'':'s')+' seen';
  }catch(e){ $('meta').textContent='disconnected'; }
  busy=false;
}

function focusTag(){
  if(focusEpc){ const m=S.t.find(x=>x.e===focusEpc); if(m) return m; }
  return S.t.length? S.t.reduce((a,b)=>b.p>a.p?b:a) : null;
}

function render(){
  $('status').textContent=S.scan?'SCANNING':'IDLE';
  $('status').className='pill '+(S.scan?'on':'off');
  $('btnScan').textContent=S.scan?'STOP':'START';
  $('btnScan').className='scan '+(S.scan?'on':'off');
  if(document.activeElement!==$('pwr')){ $('pwr').value=S.pwr; }
  $('pwrVal').textContent=S.pwr;

  const f=focusTag();
  if(f){
    const p=f.p/100, lost=f.a>2000;
    $('fepc').textContent=f.e+(focusEpc?'  (locked)':'  (strongest)');
    $('fepc').className='epc'+(lost?' lost':'');
    $('fprox').innerHTML=(lost?'—':f.p)+'<small>%</small>';
    $('fbar').style.width=(lost?0:f.p)+'%';
    $('fbar').style.background=colour(p);
    $('frssi').textContent=f.r; $('frate').textContent=f.h.toFixed(1);
    $('freads').textContent=f.c;
  }else{
    $('fepc').textContent=S.scan?'— searching —':'— tap START —';
    $('fepc').className='epc';
    $('fprox').innerHTML='0<small>%</small>'; $('fbar').style.width='0';
    $('frssi').textContent='—';$('frate').textContent='—';$('freads').textContent='—';
  }

  const L=$('list');
  if(!S.t.length){ L.innerHTML='<div class="empty">No tags detected yet</div>'; return; }
  L.innerHTML=S.t.map(x=>
    '<div class="tag'+(x.e===focusEpc?' sel':'')+'" data-e="'+x.e+'">'+
    '<span class="id">'+x.e+'</span>'+
    '<span class="mini"><i style="width:'+x.p+'%;background:'+colour(x.p/100)+'"></i></span>'+
    '<span class="db">'+x.r+' dB</span></div>').join('');
  [...L.children].forEach(el=>el.onclick=()=>{
    focusEpc = (focusEpc===el.dataset.e)? null : el.dataset.e; render();
  });
}

// --- clicks (Web Audio; needs a tap to start, per browser policy) ---
function tick(t){
  const o=actx.createOscillator(), g=actx.createGain();
  o.frequency.value=3000; o.connect(g); g.connect(actx.destination);
  g.gain.setValueAtTime(0.0001,t);
  g.gain.exponentialRampToValueAtTime(0.3,t+0.001);
  g.gain.exponentialRampToValueAtTime(0.0001,t+0.03);
  o.start(t); o.stop(t+0.04);
}
setInterval(()=>{
  if(!audioOn||!actx||!S.scan) return;
  const f=focusTag(); const p=(f&&f.a<2000)?f.p/100:0;
  if(p<=0){ nextTick=actx.currentTime+0.1; return; }
  const hz=0.5+p*14.5, horizon=actx.currentTime+0.3;
  while(nextTick<horizon){
    if(nextTick<actx.currentTime) nextTick=actx.currentTime+0.01;
    tick(nextTick); nextTick+=1/hz;
  }
},100);

$('btnScan').onclick=()=>{ S.scan=!S.scan; render(); api(S.scan?'/api/start':'/api/stop'); };
$('pwr').oninput=e=>{ $('pwrVal').textContent=e.target.value; };
$('pwr').onchange=e=>api('/api/power?dbm='+e.target.value);
$('btnClear').onclick=()=>{ focusEpc=null; api('/api/clear'); };
$('btnAudio').onclick=e=>{
  if(!actx) actx=new (window.AudioContext||window.webkitAudioContext)();
  if(actx.state==='suspended') actx.resume();
  audioOn=!audioOn;
  e.target.textContent='CLICKS: '+(audioOn?'ON':'OFF');
  e.target.className='half'+(audioOn?' act':'');
};

poll(); setInterval(poll,200);
</script></body></html>
)HTMLPAGE";
