#!/usr/bin/env node
const { spawnSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const args = process.argv.slice(2);
const cmd = args[0] || 'help';

function run(bin, extra=[]) {
  const p = path.join(__dirname, '..', 'build', bin);
  const exe = fs.existsSync(p) ? p : p + '.exe';
  const finalExe = fs.existsSync(exe) ? exe : p;
  if (!fs.existsSync(finalExe) && !fs.existsSync(p)) {
    console.log(`Buildando MGD antes de rodar ${bin}...`);
    spawnSync('bash', ['build.sh'], { stdio: 'inherit', cwd: path.join(__dirname, '..') });
  }
  const toRun = fs.existsSync(exe) ? exe : p;
  const res = spawnSync(toRun, extra, { stdio: 'inherit' });
  process.exit(res.status ?? 0);
}

if (cmd === 'test') run('mgd_tests');
else if (cmd === 'headless') {
  const folder = args[1];
  run('mgd_app', folder ? [folder] : []);
} else if (cmd === 'scan' && args[1]) {
  run('mgd_app', [args[1]]);
} else {
  console.log(`
brunorato — MGD Engine (terminal, sem UI)

  npx brunorato test                # 10/10 testes
  npx brunorato headless            # cena mock → output.ppm
  npx brunorato headless ./Skyrim   # escaneia Skyrim LE e renderiza
  npx brunorato scan ./Skyrim       # alias para headless com pasta

  npm install -g brunorato && brunorato headless
  `);
}
