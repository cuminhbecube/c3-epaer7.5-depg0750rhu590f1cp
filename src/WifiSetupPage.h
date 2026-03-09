#ifndef WIFI_SETUP_PAGE_H
#define WIFI_SETUP_PAGE_H

// Inject vào <head> của WiFiManager portal — dark theme khớp với Image Mode
const char WM_HEAD_EXTRA[] PROGMEM = R"raw(
<style>
:root{
  --bg:#0d1117;--surface:#161b22;--surface2:#21262d;
  --border:#30363d;--text:#e6edf3;--muted:#8b949e;
  --accent:#58a6ff;--red:#f85149;--green:#3fb950
}
*,*::before,*::after{box-sizing:border-box}
body{
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
  background:var(--bg)!important;color:var(--text)!important;margin:0;padding:16px
}
/* Wrapper */
div[class="wrap"]{max-width:500px;margin:0 auto}
/* Header / title */
h1,h2,h3{color:var(--text)!important}
h1{text-align:center;font-size:1.4rem;margin-bottom:4px}
/* Info banner */
.info,.wifimsg,.msg{
  background:rgba(88,166,255,0.08)!important;
  border:1px solid rgba(88,166,255,0.22)!important;
  color:#79c0ff!important;
  border-radius:10px!important;padding:12px 15px!important;
  font-size:0.84rem!important;margin:10px 0!important
}
/* Form container */
form{
  background:var(--surface)!important;
  border:1px solid var(--border)!important;
  border-radius:12px!important;padding:20px!important;margin:12px 0!important
}
/* Labels */
label{color:var(--muted)!important;font-size:0.82rem!important;display:block;margin-bottom:4px}
/* Inputs */
input[type=text],input[type=password],input[type=number],select,textarea{
  width:100%!important;padding:10px 12px!important;
  background:var(--surface2)!important;
  border:1px solid var(--border)!important;
  border-radius:8px!important;color:var(--text)!important;
  font-size:0.9rem!important;outline:none!important;
  transition:border-color .2s!important;margin-bottom:12px!important
}
input:focus,select:focus{border-color:var(--accent)!important}
/* Button */
button,input[type=submit],.btn{
  width:100%!important;padding:11px!important;
  background:var(--accent)!important;color:#0d1117!important;
  border:none!important;border-radius:9px!important;
  font-size:0.9rem!important;font-weight:700!important;
  cursor:pointer!important;transition:opacity .15s!important;margin-top:4px!important
}
button:hover,input[type=submit]:hover{opacity:0.85!important}
/* WiFi list items */
.wifi_item,.wc{
  background:var(--surface)!important;
  border:1px solid var(--border)!important;
  border-radius:8px!important;padding:11px 14px!important;
  margin:6px 0!important;display:flex;align-items:center;gap:10px;
  cursor:pointer!important;transition:border-color .2s!important
}
.wifi_item:hover,.wc:hover{border-color:var(--accent)!important}
.rssi{color:var(--muted)!important;font-size:0.78rem!important}
/* Separator */
hr{border:none!important;border-top:1px solid var(--border)!important;margin:14px 0!important}
/* Links */
a{color:var(--accent)!important;text-decoration:none!important}
a:hover{text-decoration:underline!important}
/* Nav menu */
.menu,.nav{
  display:flex;flex-wrap:wrap;gap:7px;
  list-style:none!important;margin:0 0 14px!important;padding:0!important
}
.menu a,.nav a{
  display:inline-block;padding:6px 14px;
  background:var(--surface2)!important;
  border:1px solid var(--border)!important;
  border-radius:99px!important;font-size:0.8rem!important;
  color:var(--muted)!important
}
.menu a:hover,.nav a:hover{border-color:var(--accent)!important;color:var(--accent)!important;text-decoration:none!important}
/* Override WM inline bg */
body>div{background:transparent!important}
</style>
<script>
// Thêm header card khi DOM sẵn sàng
document.addEventListener('DOMContentLoaded',function(){
  var b=document.body;
  var hdr=document.createElement('div');
  hdr.style.cssText='text-align:center;padding:18px 0 10px';
  hdr.innerHTML='<div style="font-size:2rem;margin-bottom:6px">&#128246;</div>'
    +'<h1 style="font-size:1.35rem;font-weight:700;margin:0">BECUBE CLOCK</h1>'
    +'<p style="color:#8b949e;font-size:0.8rem;margin:3px 0 0">ESP32-C3 &middot; 7.5&quot; E-Paper &middot; WiFi Setup</p>';
  var wrap=document.querySelector('div');
  if(wrap)wrap.insertBefore(hdr,wrap.firstChild);
  else b.insertBefore(hdr,b.firstChild);
});
</script>
)raw";

#endif // WIFI_SETUP_PAGE_H
