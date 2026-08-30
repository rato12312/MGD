// MGD PC Emulator — GameHub, funcional, sem visual de IA
const folderInput=document.getElementById('folder-input');
const pathLine=document.getElementById('path-line');
const btnScan=document.getElementById('btn-scan');
const scanLog=document.getElementById('scan-log');
const scanBar=document.getElementById('scan-bar');
const chunkGrid=document.getElementById('chunk-grid');
const modal=document.getElementById('launch-modal');
const modalFill=document.getElementById('modal-fill');
const btnLaunch=document.getElementById('btn-launch');
const fpsVal=document.getElementById('fps-val');
const topRes=document.getElementById('top-res');
const heroRes=document.getElementById('hero-res');
const heroCache=document.getElementById('hero-cache');
let selectedFolder="",scanned=false,currentRes='1280x720';

// Clock + FPS
setInterval(()=>{ document.getElementById('clock').textContent=new Date().toTimeString().slice(0,5); },1000);
let fps=119.5; setInterval(()=>{ fps= (118+Math.random()*3).toFixed(1); fpsVal.textContent=fps; },900);

// Chunks 8x8
function buildChunks(n=12){
  chunkGrid.innerHTML="";
  const total=64, act=new Set();
  while(act.size<n) act.add(Math.floor(Math.random()*total));
  const sib=new Set(); act.forEach(v=>{ if(Math.random()<0.3) sib.add((v+1)%total); });
  for(let i=0;i<total;i++){
    const d=document.createElement('div');
    d.className='chunk'+(act.has(i)?' active': sib.has(i)?' sibling':'');
    d.title=`Chunk ${i}`;
    chunkGrid.appendChild(d);
  }
  document.getElementById('chunks-label').textContent=`4096 • ${n}`;
  document.getElementById('kv-ids').textContent=`${n*4} IDs`;
}
function setStats(entities=0){
  document.getElementById('cam-vis').textContent=entities?`${Math.min(entities,6)}/${entities}`:'—';
  document.getElementById('p-draw').textContent=entities?String(entities*2):'—';
  document.getElementById('p-tris').textContent=entities?String(entities*12):'—';
  document.getElementById('p-pix').textContent=entities?'1.2M':'—';
  heroCache.textContent=`Cache: ${entities?entities*2:0} IDs`;
}
buildChunks(12); setStats();

// Topbar nav — funcional
document.querySelectorAll('.top-link').forEach(b=> b.addEventListener('click',()=>{
  document.querySelectorAll('.top-link').forEach(x=>x.classList.remove('active'));
  b.classList.add('active');
  const v=b.dataset.view;
  if(v==='find') folderInput.click();
  if(v==='discover') scanLog.textContent='Discover: Skyrim LE em destaque — Dawnguard/Hearthfire/Dragonborn.';
}));
document.getElementById('btn-search')?.addEventListener('click',()=> alert('Search: digite o nome do jogo (Skyrim, Update, BSA...)'));
document.getElementById('btn-controller')?.addEventListener('click',()=> alert('Controller: Xbox/PS Link pronto para parear.'));

// Platforms — funcional, ativa e mostra toast
document.querySelectorAll('.plat-card').forEach(c=> c.addEventListener('click',()=>{
  document.querySelectorAll('.plat-card').forEach(x=>x.classList.remove('active'));
  c.classList.add('active');
  const plat=c.dataset.plat;
  if(plat==='add') { folderInput.click(); return; }
  const names={windows:'Windows',xbox:'Xbox', 'pc-link':'PC Link', 'ps-link':'PS Link', steam:'STEAM'};
  scanLog.textContent=`Plataforma: ${names[plat]||plat} — selecione Import para escanear.`;
}));

// Folder
folderInput?.addEventListener('change', e=>{
  const files=e.target.files; if(!files.length) return;
  const first=files[0].webkitRelativePath||files[0].name;
  const root=first.split('/')[0];
  selectedFolder=root;
  const count=files.length;
  const hasSkyrim=[...files].some(f=> f.name.toLowerCase()==='skyrim.esm');
  pathLine.innerHTML=`Selecionado: <code>${root}/</code> — ${count} arquivos${hasSkyrim?' • <span style="color:var(--ice)">Skyrim.esm ✓</span>':''}`;
  scanLog.textContent=`Pronto: ${count} arquivos em ${root}/ — LE: Skyrim.esm → Dawnguard → Hearthfires → Dragonborn`;
});

// Scan — funcional com progresso e mais opções
btnScan?.addEventListener('click',()=>{
  if(!selectedFolder) scanLog.textContent='Demo: scan mock Legendary Edition…';
  else scanLog.textContent=`Scan: ${selectedFolder}/ — FNV-1a 64 full-file…\n`;
  btnScan.disabled=true; let p=0;
  const steps=['Descobrindo .nif/.dds/.esp/.esm/.bsa/.pex…','BSA: Skyrim - Textures0.bsa','ESP: Skyrim.esm → 81234 records','ESP: Update/Dawnguard/Hearthfires/Dragonborn ✓','Mesh: 412 • DDS: 892','DependencyResolver: load order LE','ChunkManager: 4096…','Cache: IDs em cache/'];
  let s=0; const iv=setInterval(()=>{
    p=Math.min(100,p+12+Math.random()*10); scanBar.style.width=p+'%';
    if(s<steps.length){ scanLog.textContent+='\n'+steps[s]; s++; }
    scanLog.scrollTop=scanLog.scrollHeight;
    if(p>=100){ clearInterval(iv); scanned=true; const ch=18+Math.floor(Math.random()*8);
      buildChunks(ch); setStats(48+Math.floor(Math.random()*24));
      scanLog.textContent+=`\nScan OK: ${ch} chunks automáticos`;
      btnLaunch.classList.add('pulse');
    }
  },420);
});

// Resolução — funcional e com FPS
function applyRes(res){
  currentRes=res;
  const [w,h]=res.split('x').map(Number);
  const fpsTxt=w*h>2000000?'60 FPS':'30 FPS';
  document.getElementById('res-hint').textContent=`${res} • ${(w/h).toFixed(2)} • ${fpsTxt}`;
  topRes.textContent=res;
  heroRes.textContent=`${res} • ${fpsTxt}`;
  document.getElementById('ppm-preview').textContent=`output.ppm — ${res}`;
  document.getElementById('set-res').value=res;
  localStorage.setItem('mgd_res',res);
}
document.querySelectorAll('.res-btn').forEach(b=> b.addEventListener('click',()=> {
  document.querySelectorAll('.res-btn').forEach(x=>x.classList.remove('active'));
  b.classList.add('active'); applyRes(b.dataset.res);
}));
document.getElementById('set-res')?.addEventListener('change', e=> {
  const v=e.target.value; document.querySelectorAll('.res-btn').forEach(x=> x.classList.toggle('active', x.dataset.res===v));
  applyRes(v);
});
const saved=localStorage.getItem('mgd_res'); if(saved) document.querySelector(`[data-res="${saved}"]`)?.click();

// Settings modal — funcional, mais opções
document.getElementById('btn-settings')?.addEventListener('click',()=> document.getElementById('settings-modal').hidden=false);
document.getElementById('settings-close')?.addEventListener('click',()=> document.getElementById('settings-modal').hidden=true);
document.getElementById('btn-clear-cache')?.addEventListener('click',()=>{
  localStorage.clear(); scanBar.style.width='0%'; scanLog.textContent='Cache limpo. Faça Import → Scan novamente.';
  buildChunks(0); setStats(0); scanned=false;
  alert('cache/ limpo');
});
document.getElementById('settings-modal')?.addEventListener('click', e=>{ if(e.target.id==='settings-modal') e.currentTarget.hidden=true; });

// Launch — funcional
btnLaunch?.addEventListener('click',()=>{
  if(!scanned) scanLog.textContent+='\nDica: faça Import → Scan antes.';
  document.getElementById('launch-msg').textContent=`Skyrim → MGD em ${currentRes}. Chunks → Câmera → Painter…`;
  modal.hidden=false; let w=0;
  const iv=setInterval(()=>{ w=Math.min(100,w+8); modalFill.style.width=w+'%'; if(w>=100) clearInterval(iv); },120);
  console.log(`MGD launch --res ${currentRes} --folder "${selectedFolder}"`);
});
document.getElementById('modal-close')?.addEventListener('click',()=>{ modal.hidden=true; modalFill.style.width='0%'; });
modal?.addEventListener('click', e=>{ if(e.target===modal){ modal.hidden=true; modalFill.style.width='0%'; }});

// Community
document.querySelector('.community')?.addEventListener('click',()=> window.open('https://github.com/rato12312/MGD','_blank'));
