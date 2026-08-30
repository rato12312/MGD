// MGD Launcher — PS5/GameHub • obra-prima
const folderInput = document.getElementById('folder-input');
const pathLine = document.getElementById('path-line');
const btnScan = document.getElementById('btn-scan');
const btnLaunch = document.getElementById('btn-launch');
const scanLog = document.getElementById('scan-log');
const scanBar = document.getElementById('scan-bar');
const chunkGrid = document.getElementById('chunk-grid');
const modal = document.getElementById('launch-modal');
const modalFill = document.getElementById('modal-fill');

let selectedFolder = "";
let scanned = false;

// Chunk grid 8x8 = 64 chunks demo
function buildChunks(activeCount=18) {
  chunkGrid.innerHTML = "";
  const total = 64;
  const actives = new Set();
  while (actives.size < activeCount) actives.add(Math.floor(Math.random()*total));
  const siblings = new Set();
  actives.forEach(v=> { if (Math.random()<0.35) siblings.add((v+1)%total); });
  for (let i=0;i<total;i++){
    const d=document.createElement('div');
    d.className='chunk'+(actives.has(i)?' active': siblings.has(i)?' sibling':'');
    d.title = `Chunk ${i} — ${actives.has(i)?'ativo':'vazio'}`;
    chunkGrid.appendChild(d);
  }
  document.getElementById('chunks-label').textContent = `4096 × 4096 • ${activeCount} chunks`;
  document.getElementById('stat-chunks').textContent = activeCount;
}

function setStats(entities=0, hits=0){
  document.getElementById('stat-entities').textContent = entities || '—';
  document.getElementById('stat-cache').textContent = hits || '—';
  document.getElementById('kv-ids').textContent = entities ? `${entities} IDs` : '—';
  document.getElementById('kv-files').textContent = entities ? `${Math.floor(entities*1.8)} arquivos` : '—';
  document.getElementById('kv-scan').textContent = scanned ? new Date().toLocaleTimeString() : '—';
  document.getElementById('p-draw').textContent = entities? String(entities*2):'—';
  document.getElementById('p-tris').textContent = entities? String(entities*12):'—';
  document.getElementById('p-pix').textContent = entities? '1.2M':'—';
  document.getElementById('cam-vis').textContent = entities? `${Math.min(entities,6)} / ${entities}`:'—';
}

buildChunks(12);
setStats();

// Folder input (webkitdirectory)
folderInput?.addEventListener('change', e=>{
  const files = e.target.files;
  if (!files.length) return;
  const first = files[0].webkitRelativePath || files[0].name;
  const root = first.split('/')[0];
  selectedFolder = root;
  const count = files.length;
  const hasSkyrim = Array.from(files).some(f=> f.name.toLowerCase()==='skyrim.esm');
  pathLine.innerHTML = `Selecionado: <code>${root}/</code> — ${count} arquivos${hasSkyrim?' • <span style="color:var(--success)">Skyrim.esm encontrado</span>':''}`;
  btnScan.disabled = false;
  scanLog.textContent = `Pronto para scan: ${count} arquivos em ${root}/ (Legendary Edition: Skyrim.esm → Dawnguard.esm → Hearthfires.esm → Dragonborn.esm)`;
});

// Scan simulation (obra-prima: cache por IDs, chunks)
btnScan?.addEventListener('click', ()=>{
  if (!selectedFolder) {
    // Demo sem pasta: usa mock
    scanLog.textContent = 'Modo demo: scan mock Legendary Edition (sem pasta)…';
  } else {
    scanLog.textContent = `Scan: ${selectedFolder}/ — BSA/ESP com FNV-1a 64 full-file…\n`;
  }
  btnScan.disabled = true;
  let p=0;
  const steps = [
    'Descobrindo arquivos .nif/.dds/.esp/.esm/.bsa/.pex…',
    'BSAAnalyzer: Skyrim - Textures0.bsa (2.1GB) ✓',
    'ESPAnalyzer: Skyrim.esm → 81234 records',
    'ESPAnalyzer: Update.esm, Dawnguard.esm, Hearthfires.esm, Dragonborn.esm ✓',
    'MeshAnalyzer: 412 meshes • TextureAnalyzer: 892 DDS',
    'DependencyResolver: ordenando masters (load order LE)',
    'ChunkManager: distribuindo em chunks 4096…',
    'Cache: gravando IDs em cache/ (sem copiar assets)'
  ];
  let s=0;
  const iv=setInterval(()=>{
    p=Math.min(100, p+ 12+Math.random()*10);
    scanBar.style.width=p+'%';
    if (s < steps.length) { scanLog.textContent += '\n'+steps[s]; s++; }
    scanLog.scrollTop=scanLog.scrollHeight;
    if (p>=100){ clearInterval(iv); scanned=true;
      const ch = 18 + Math.floor(Math.random()*8);
      buildChunks(ch);
      setStats(48+Math.floor(Math.random()*24), Math.floor(Math.random()*40)+60);
      scanLog.textContent += `\nScan OK: ${ch} chunks automáticos, 48 entities por IDs`;
      document.getElementById('chunks-label').textContent = `4096 × 4096 • ${ch} chunks`;
      btnLaunch.classList.add('pulse');
    }
  }, 420);
});

// Carousel
const carousel=document.getElementById('carousel');
document.getElementById('car-prev')?.addEventListener('click', ()=> carousel.scrollBy({left:-280, behavior:'smooth'}));
document.getElementById('car-next')?.addEventListener('click', ()=> carousel.scrollBy({left:280, behavior:'smooth'}));
carousel?.addEventListener('click', e=>{
  const card=e.target.closest('.game-card');
  if(!card) return;
  document.querySelectorAll('.game-card').forEach(c=>c.classList.remove('active'));
  card.classList.add('active');
});

// Resolução
let currentRes = '1280x720';
document.querySelectorAll('.res-btn').forEach(b=> b.addEventListener('click', ()=>{
  document.querySelectorAll('.res-btn').forEach(x=>x.classList.remove('active'));
  b.classList.add('active');
  currentRes = b.dataset.res;
  const [w,h] = currentRes.split('x').map(Number);
  const fps = w*h > 2000000 ? '60 FPS' : '30 FPS';
  document.getElementById('res-hint').textContent = `${currentRes} • ${ (w/h).toFixed(2)} • ${fps}`;
  // Atualiza preview do painter
  document.getElementById('ppm-preview').textContent = `output.ppm — ${currentRes} • aguardando headless`;
  localStorage.setItem('mgd_res', currentRes);
}));
const savedRes = localStorage.getItem('mgd_res');
if (savedRes) document.querySelector(`[data-res="${savedRes}"]`)?.click();

// Launch com resolução
btnLaunch?.addEventListener('click', ()=>{
  if (!scanned) { scanLog.textContent += '\nDica: faça o Scan antes de rodar o MGD.'; }
  modal.querySelector('p').textContent = `Skyrim → MGD em ${currentRes}. Mental Map → Chunks → Câmera guiada por colisão → Painter pintando…`;
  modal.hidden=false;
  let w=0;
  const iv=setInterval(()=>{ w=Math.min(100,w+8); modalFill.style.width=w+'%'; if(w>=100) clearInterval(iv); }, 120);
  console.log(`MGD launch: mgd_app.exe --res ${currentRes} --folder "${selectedFolder}"`);
});
document.getElementById('modal-close')?.addEventListener('click', ()=>{ modal.hidden=true; modalFill.style.width='0%'; });
modal?.addEventListener('click', e=>{ if(e.target===modal) { modal.hidden=true; modalFill.style.width='0%'; }});

// Sidenav demo
document.querySelectorAll('.nav-item').forEach(b=> b.addEventListener('click', ()=>{
  document.querySelectorAll('.nav-item').forEach(x=>x.classList.remove('active'));
  b.classList.add('active');
}));
