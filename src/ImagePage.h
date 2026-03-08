#ifndef IMAGE_PAGE_H
#define IMAGE_PAGE_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <title>E-Paper Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    *,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
    :root{
      --bg:#0d1117;--surface:#161b22;--surface2:#21262d;
      --border:#30363d;--text:#e6edf3;--muted:#8b949e;
      --accent:#58a6ff;--red:#f85149;--green:#3fb950;--r:12px
    }
    body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh;padding:16px}
    .wrap{max-width:680px;margin:0 auto}
    /* Header */
    .hdr{text-align:center;padding:20px 0 16px}
    .hdr h1{font-size:1.4rem;font-weight:700}
    .hdr p{color:var(--muted);font-size:0.82rem;margin-top:3px}
    /* Banner */
    .banner{display:flex;align-items:center;gap:14px;background:rgba(88,166,255,0.07);border:1px solid rgba(88,166,255,0.2);border-radius:var(--r);padding:14px 16px;margin-bottom:14px}
    .banner-icon{font-size:1.6rem;flex-shrink:0}
    .banner h3{font-size:0.88rem;font-weight:600;color:var(--accent)}
    .banner p{font-size:0.78rem;color:var(--muted);margin-top:2px}
    .banner code{font-size:0.82rem;background:var(--surface2);padding:1px 6px;border-radius:5px;color:var(--text)}
    /* Card */
    .card{background:var(--surface);border:1px solid var(--border);border-radius:var(--r);margin-bottom:14px;overflow:hidden}
    .card-hdr{padding:12px 16px;border-bottom:1px solid var(--border);display:flex;align-items:center;gap:8px}
    .card-hdr h2{font-size:0.88rem;font-weight:600;flex:1}
    .badge{font-size:0.7rem;background:var(--surface2);border:1px solid var(--border);color:var(--muted);padding:2px 9px;border-radius:99px}
    .card-body{padding:16px}
    /* Specs */
    .specs{display:flex;gap:7px;flex-wrap:wrap;margin-bottom:14px}
    .chip{font-size:0.73rem;padding:4px 11px;background:var(--surface2);border:1px solid var(--border);border-radius:99px;color:var(--muted)}
    .chip b{color:var(--accent)}
    /* Drop zone */
    .drop{border:2px dashed var(--border);border-radius:10px;padding:26px 16px;text-align:center;cursor:pointer;transition:border-color .2s,background .2s;position:relative;margin-bottom:12px}
    .drop:hover,.drop.drag{border-color:var(--accent);background:rgba(88,166,255,0.05)}
    .drop input{position:absolute;inset:0;opacity:0;cursor:pointer;width:100%;height:100%}
    .drop-ico{font-size:2rem;margin-bottom:7px}
    .drop p{font-size:0.84rem;color:var(--muted)}
    .drop a{color:var(--accent);font-weight:600}
    .fname{font-size:0.8rem;color:var(--accent);font-weight:600;margin-top:7px;display:none}
    /* Status */
    .status{padding:10px 14px;border-radius:8px;font-size:0.82rem;font-weight:500;display:none;margin-bottom:11px}
    .status.info{background:rgba(88,166,255,0.1);border:1px solid rgba(88,166,255,0.2);color:#79c0ff}
    .status.success{background:rgba(63,185,80,0.1);border:1px solid rgba(63,185,80,0.2);color:#56d364}
    .status.error{background:rgba(248,81,73,0.1);border:1px solid rgba(248,81,73,0.2);color:#ff7b72}
    /* Progress */
    .prog{margin:10px 0;display:none}
    .prog-track{height:6px;background:var(--surface2);border-radius:99px;overflow:hidden}
    .prog-bar{height:100%;width:0%;background:var(--accent);border-radius:99px;transition:width .2s}
    .prog-info{display:flex;justify-content:space-between;font-size:0.72rem;color:var(--muted);margin-top:5px}
    /* Button */
    .btn-up{display:flex;align-items:center;justify-content:center;gap:7px;width:100%;padding:11px;border:none;border-radius:9px;font-size:0.9rem;font-weight:700;cursor:pointer;background:var(--accent);color:#0d1117;transition:opacity .15s,transform .1s}
    .btn-up:hover:not(:disabled){opacity:0.85}
    .btn-up:active:not(:disabled){transform:scale(0.98)}
    .btn-up:disabled{background:var(--surface2);color:var(--muted);cursor:not-allowed}
    /* Preview */
    #previewCard{display:none}
    #previewImage{width:100%;border-radius:8px;border:1px solid var(--border);display:block}
    .note{font-size:0.72rem;color:var(--muted);margin-top:7px}
    /* Slots */
    .slots{display:flex;flex-direction:column;gap:7px}
    .slot{display:flex;align-items:center;gap:11px;padding:10px 14px;background:var(--surface2);border-radius:8px;border:1px solid var(--border)}
    .slot-n{width:30px;height:30px;background:rgba(88,166,255,0.12);border-radius:7px;display:flex;align-items:center;justify-content:center;font-size:0.8rem;font-weight:700;color:var(--accent);flex-shrink:0}
    .slot-meta{flex:1;font-size:0.82rem;color:var(--muted)}
    .slot-meta b{color:var(--text)}
    .btn-del{font-size:0.78rem;padding:5px 12px;background:rgba(248,81,73,0.1);color:var(--red);border:1px solid rgba(248,81,73,0.25);border-radius:6px;cursor:pointer;transition:background .2s;flex-shrink:0}
    .btn-del:hover{background:rgba(248,81,73,0.25)}
    .empty{text-align:center;padding:18px;color:var(--muted);font-size:0.82rem}
    canvas{display:none}
  </style>
</head>
<body>
<div class="wrap">
  <div class="hdr">
    <h1>&#128444; E-Paper Manager</h1>
    <p id="subtitle">ESP32-C3 &middot; 7.5&quot; BWR Display</p>
  </div>

  <div class="banner">
    <div class="banner-icon">&#128246;</div>
    <div>
      <h3>Connected: BECUBE-IMG</h3>
      <p>If page doesn't load, open <code>http://192.168.4.1</code></p>
    </div>
  </div>

  <div class="card">
    <div class="card-hdr">
      <span>&#8679;</span>
      <h2>Upload Image</h2>
      <span class="badge" id="specBadge">640 &times; 384</span>
    </div>
    <div class="card-body">
      <div class="specs">
        <div class="chip">Size <b id="specRes">640 &times; 384</b></div>
        <div class="chip">Colors <b>BW + Red</b></div>
        <div class="chip">Binary <b>61440 B</b></div>
      </div>
      <div class="drop" id="dropzone">
        <input type="file" id="fileInput" accept="image/*" onchange="processImage()">
        <div class="drop-ico">&#128193;</div>
        <p>Drag &amp; drop or <a>tap to choose</a></p>
        <div class="fname" id="fileName"></div>
      </div>
      <canvas id="canvas" width="640" height="384"></canvas>
      <div id="statusBox" class="status"></div>
      <div class="prog" id="progressWrap">
        <div class="prog-track"><div class="prog-bar" id="progressFill"></div></div>
        <div class="prog-info"><span id="progressText">Uploading...</span><span id="progressPct">0%</span></div>
      </div>
      <button id="uploadBtn" class="btn-up" onclick="uploadImage()" disabled>
        &#8679; Upload to Display
      </button>
    </div>
  </div>

  <div class="card" id="previewCard">
    <div class="card-hdr">
      <span>&#128065;</span>
      <h2>Preview</h2>
      <span class="badge">Dithered output</span>
    </div>
    <div class="card-body">
      <img id="previewImage">
      <p class="note">Colors shown match what will appear on the E-Ink panel</p>
    </div>
  </div>

  <div class="card">
    <div class="card-hdr">
      <span>&#128247;</span>
      <h2>Stored Images</h2>
      <span class="badge" id="imageCountBadge">0 / 5</span>
    </div>
    <div class="card-body">
      <div id="imageList" class="slots">
        <div class="empty">Loading...</div>
      </div>
    </div>
  </div>
</div>
<script>
let WIDTH = 640;
let HEIGHT = 384;
let currentDeg = 0;
let processedData = null;

const CFG = {
    SHADOW_FLOOR:  70,
    BLACK_PENALTY: 2.0,
};

function setStatus(msg, type) {
    const box = document.getElementById('statusBox');
    box.textContent = msg;
    box.className = 'status ' + (type || 'info');
    box.style.display = msg ? 'block' : 'none';
}

function processImage() {
    const fileInput = document.getElementById('fileInput');
    const uploadBtn = document.getElementById('uploadBtn');
    if (!fileInput.files[0]) return;
    const file = fileInput.files[0];
    document.getElementById('fileName').style.display = 'block';
    document.getElementById('fileName').textContent = file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)';
    document.getElementById('previewCard').style.display = 'none';
    setStatus('Processing image...', 'info');
    uploadBtn.disabled = true;
    const reader = new FileReader();
    reader.onload = function(event) {
        const img = new Image();
        img.onload = function() {
            const canvas = document.getElementById('canvas');
            const ctx = canvas.getContext('2d');
            ctx.fillStyle = 'white';
            ctx.fillRect(0, 0, WIDTH, HEIGHT);
            const scale = Math.max(WIDTH / img.width, HEIGHT / img.height);
            const x = (WIDTH - img.width * scale) / 2;
            const y = (HEIGHT - img.height * scale) / 2;
            ctx.drawImage(img, x, y, img.width * scale, img.height * scale);
            const imageData = ctx.getImageData(0, 0, WIDTH, HEIGHT);
            dither(imageData);
            ctx.putImageData(imageData, 0, 0);
            document.getElementById('previewImage').src = canvas.toDataURL();
            document.getElementById('previewCard').style.display = 'block';
            processedData = generateBitstream(imageData);
            setStatus('Ready \u2014 ' + processedData.length + ' bytes. Click Upload to send.', 'success');
            uploadBtn.disabled = false;
        };
        img.onerror = function() { setStatus('Error: Failed to load image', 'error'); };
        img.src = event.target.result;
    };
    reader.onerror = function() { setStatus('Error: Failed to read file', 'error'); };
    reader.readAsDataURL(file);
}

function dither(imageData) {
    const w = imageData.width, h = imageData.height, d = imageData.data;
    const { SHADOW_FLOOR, BLACK_PENALTY } = CFG;
    if (SHADOW_FLOOR > 0) {
        for (let i = 0; i < d.length; i += 4) {
            d[i]   = Math.max(d[i],   SHADOW_FLOOR);
            d[i+1] = Math.max(d[i+1], SHADOW_FLOOR);
            d[i+2] = Math.max(d[i+2], SHADOW_FLOOR);
        }
    }
    for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
            const i = (y * w + x) * 4;
            const lum = 0.299 * d[i] + 0.587 * d[i+1] + 0.114 * d[i+2];
            const isBlack = lum < (127.5 / BLACK_PENALTY);
            const newVal  = isBlack ? 0 : 255;
            d[i] = newVal; d[i+1] = newVal; d[i+2] = newVal;
            const err = lum - newVal;
            const f = 1/8;
            distributeGray(d, x+1, y,   w, h, err, f);
            distributeGray(d, x+2, y,   w, h, err, f);
            distributeGray(d, x-1, y+1, w, h, err, f);
            distributeGray(d, x,   y+1, w, h, err, f);
            distributeGray(d, x+1, y+1, w, h, err, f);
            distributeGray(d, x,   y+2, w, h, err, f);
        }
    }
}

function distributeGray(d, x, y, w, h, err, factor) {
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    const i = (y * w + x) * 4;
    const v = err * factor;
    d[i]   = Math.min(255, Math.max(0, d[i]   + v));
    d[i+1] = Math.min(255, Math.max(0, d[i+1] + v));
    d[i+2] = Math.min(255, Math.max(0, d[i+2] + v));
}

function generateBitstream(imageData) {
    // Always output native physical panel format: 640 × 384
    // For portrait (90°/270°), pixels are remapped from the rotated canvas to native coords.
    const NATIVE_W = 640;
    const NATIVE_H = 384;
    const size = (NATIVE_W * NATIVE_H) / 8;  // 30720 bytes per plane
    const blackBuffer = new Uint8Array(size);
    const redBuffer   = new Uint8Array(size);
    const d = imageData.data;

    if (currentDeg === 90 || currentDeg === 270) {
        // Canvas is WIDTH(384) x HEIGHT(640)
        // Remap each canvas pixel to its native physical position
        for (let py = 0; py < HEIGHT; py++) {
            for (let px = 0; px < WIDTH; px++) {
                const lum = d[(py * WIDTH + px) * 4];
                if (lum < 128) continue; // black pixels: buffers already 0
                let nx, ny;
                if (currentDeg === 90) {
                    // Adafruit GFX rotation=1 (90° CW): canvas(cx,cy) -> native(639-cy, cx)
                    nx = NATIVE_W - 1 - py;
                    ny = px;
                } else {
                    // Adafruit GFX rotation=3 (270° CW): canvas(cx,cy) -> native(cy, 383-cx)
                    nx = py;
                    ny = NATIVE_H - 1 - px;
                }
                const byteIdx = Math.floor((ny * NATIVE_W + nx) / 8);
                const bitIdx  = 7 - ((ny * NATIVE_W + nx) % 8);
                blackBuffer[byteIdx] |= (1 << bitIdx);
                redBuffer[byteIdx]   |= (1 << bitIdx);
            }
        }
    } else {
        // Landscape (0° or 180°): canvas is already 640×384, direct mapping
        for (let y = 0; y < NATIVE_H; y++) {
            for (let x = 0; x < NATIVE_W; x++) {
                const lum = d[(y * NATIVE_W + x) * 4];
                if (lum < 128) continue;
                const byteIdx = Math.floor((y * NATIVE_W + x) / 8);
                const bitIdx  = 7 - ((y * NATIVE_W + x) % 8);
                blackBuffer[byteIdx] |= (1 << bitIdx);
                redBuffer[byteIdx]   |= (1 << bitIdx);
            }
        }
    }

    const combined = new Uint8Array(size * 2);
    combined.set(blackBuffer);
    combined.set(redBuffer, size);
    return combined;
}

function uploadImage() {
    if (!processedData) { setStatus('No image data \u2014 please select an image first.', 'error'); return; }
    const uploadBtn    = document.getElementById('uploadBtn');
    const progressWrap = document.getElementById('progressWrap');
    const progressFill = document.getElementById('progressFill');
    const progressPct  = document.getElementById('progressPct');
    const progressText = document.getElementById('progressText');
    uploadBtn.disabled = true;
    progressWrap.style.display = 'block';
    setStatus('Uploading...', 'info');
    const CHUNK_SIZE = 4096;
    const totalBytes = processedData.length;
    let offset = 0;
    async function sendNextChunk() {
        if (offset >= totalBytes) {
            try {
                let response = await fetch('/upload_finish', { method: 'POST' });
                if (response.ok) {
                    progressFill.style.width = '100%';
                    progressPct.textContent = '100%';
                    progressText.textContent = 'Done!';
                    try {
                        const d = await response.json();
                        setStatus('\u2713 Saved to slot ' + (d.slot+1) + ' (' + d.total + '/5 images stored)', 'success');
                    } catch(e) { setStatus('\u2713 Upload complete \u2014 display updating now', 'success'); }
                    loadImages();
                } else {
                    setStatus('Error finishing upload (server ' + response.status + ')', 'error');
                    uploadBtn.disabled = false;
                }
            } catch (err) {
                setStatus('Network error during finish: ' + err.message, 'error');
                uploadBtn.disabled = false;
            }
            return;
        }
        const end = Math.min(offset + CHUNK_SIZE, totalBytes);
        const chunkBlob = processedData.subarray(offset, end);
        const percent = Math.round((offset / totalBytes) * 100);
        progressFill.style.width = percent + '%';
        progressPct.textContent = percent + '%';
        progressText.textContent = 'Uploading ' + (offset/1024).toFixed(1) + ' / ' + (totalBytes/1024).toFixed(1) + ' KB';
        const formData = new FormData();
        formData.append('file', new Blob([chunkBlob], {type: 'application/octet-stream'}), 'chunk.bin');
        try {
            let response = await fetch('/upload_chunk?offset=' + offset, { method: 'POST', body: formData });
            if (response.ok) { offset += CHUNK_SIZE; setTimeout(sendNextChunk, 50); }
            else { throw new Error('Server ' + response.status); }
        } catch (error) {
            setStatus('Error: ' + error.message + ' \u2014 retrying...', 'error');
            setTimeout(sendNextChunk, 1000);
        }
    }
    sendNextChunk();
}

async function loadImages() {
    try {
        const r = await fetch('/images');
        if (!r.ok) return;
        const data = await r.json();
        document.getElementById('imageCountBadge').textContent = data.count + ' / 5';
        const list = document.getElementById('imageList');
        if (!data.images || data.images.length === 0) {
            list.innerHTML = '<div class="empty">No images stored yet</div>';
            return;
        }
        list.innerHTML = data.images.map(img =>
            '<div class="slot">' +
            '<div class="slot-n">' + (img.id+1) + '</div>' +
            '<div class="slot-meta"><b>Slot ' + (img.id+1) + '</b> &nbsp;&middot;&nbsp; ' + (img.size/1024).toFixed(1) + ' KB</div>' +
            '<button class="btn-del" onclick="deleteImage(' + img.id + ')">&#128465; Delete</button>' +
            '</div>'
        ).join('');
    } catch(e) { console.error('loadImages:', e); }
}

async function deleteImage(id) {
    if (!confirm('Delete slot ' + (id+1) + '?')) return;
    try {
        const fd = new FormData();
        fd.append('id', id);
        const r = await fetch('/delete', { method: 'POST', body: fd });
        if (r.ok) loadImages();
    } catch(e) { console.error('deleteImage:', e); }
}

async function loadRotation() {
    try {
        const r = await fetch('/rotation');
        if (!r.ok) return;
        const d = await r.json();
        WIDTH  = d.width  || 640;
        HEIGHT = d.height || 384;
        const canvas = document.getElementById('canvas');
        canvas.width = WIDTH; canvas.height = HEIGHT;
        const rs = document.getElementById('specRes');
        if (rs) rs.textContent = WIDTH + ' \u00d7 ' + HEIGHT;
        const sb = document.getElementById('specBadge');
        if (sb) sb.textContent = WIDTH + ' \u00d7 ' + HEIGHT;
        const deg = d.degrees || 0;
        currentDeg = deg;
        const subtitle = document.getElementById('subtitle');
        if (subtitle) subtitle.textContent = 'ESP32-C3 \u00b7 7.5" BWR Display \u00b7 Rotation: ' + deg + '\u00b0';
    } catch(e) { console.warn('loadRotation:', e); }
}

// Drag & drop highlight
const dz = document.getElementById('dropzone');
dz.addEventListener('dragover', e => { e.preventDefault(); dz.classList.add('drag'); });
dz.addEventListener('dragleave', () => dz.classList.remove('drag'));
dz.addEventListener('drop', e => { e.preventDefault(); dz.classList.remove('drag'); document.getElementById('fileInput').files = e.dataTransfer.files; processImage(); });

window.addEventListener('load', async () => { await loadRotation(); loadImages(); });
</script>
</body>
</html>
)rawliteral";

#endif
