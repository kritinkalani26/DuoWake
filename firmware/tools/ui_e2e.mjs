// ui_e2e.mjs - drives the real page in headless Chrome over the DevTools protocol and checks the user flows:
//   1. fresh board with no clock -> UI says "clock not set"; "Sync time from phone" fixes it (backend + UI)
//   2. change the time picker -> countdown previews the new time BEFORE saving
//   3. "Set alarm" -> backend state carries the new time, UI countdown matches, value survives polling
// Usage:  python tools/mock_server.py   and   chrome --headless=new --remote-debugging-port=9222 --user-data-dir=<tmp>
//         node tools/ui_e2e.mjs [http://127.0.0.1:8080/]
const URL = process.argv[2] || 'http://127.0.0.1:8080/';
const tabs = await (await fetch('http://127.0.0.1:' + (process.env.CDP_PORT || '9222') + '/json/new?about:blank', { method: 'PUT' })).json();
const ws = new WebSocket(tabs.webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pending = new Map(); const logs = [];
ws.onmessage = e => {
  const m = JSON.parse(e.data);
  if (m.id && pending.has(m.id)) { pending.get(m.id)(m); pending.delete(m.id); }
  if (m.method === 'Runtime.consoleAPICalled') logs.push(m.params.args.map(a => a.value ?? a.description).join(' '));
  if (m.method === 'Runtime.exceptionThrown') logs.push('EXCEPTION ' + m.params.exceptionDetails.text + ' ' + (m.params.exceptionDetails.exception?.description || ''));
};
const send = (method, params = {}) => new Promise(r => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const evalJs = async (expr) => {
  const r = await send('Runtime.evaluate', { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.result.exceptionDetails) throw new Error(r.result.exceptionDetails.text + ' ' + JSON.stringify(r.result.exceptionDetails.exception));
  return r.result.result.value;
};
const sleep = ms => new Promise(r => setTimeout(r, ms));
const text = id => evalJs(`document.getElementById('${id}').textContent`);
const backend = async () => (await fetch(URL + 'api/state')).json();
const fmt = secs => { const h = Math.floor(secs / 3600), m = Math.floor((secs % 3600) / 60); return h > 0 ? `${h}h ${String(m).padStart(2, '0')}m` : `${m}m`; };
const results = [];
const check = (name, ok, info) => { results.push(ok); console.log((ok ? 'PASS ' : 'FAIL ') + name + (info ? '  -> ' + info : '')); };

await send('Runtime.enable'); await send('Page.enable');
await send('Page.navigate', { url: URL }); await sleep(2500);

// ---- 1. unsynced clock + sync ----
const b0 = await backend();
const r0 = await evalJs(`({cd: document.getElementById('countdown').textContent, dev: document.getElementById('devtime').textContent, games: document.querySelectorAll('.gcard').length, tunes: document.querySelectorAll('.tune').length})`);
check('page rendered options', r0.games >= 3 && r0.tunes >= 3, JSON.stringify(r0));
if (b0.timeSource === 'millis') {
  check('unsynced board is flagged in the UI', /clock not set/i.test(r0.cd) && /clock not set/i.test(r0.dev), `cd="${r0.cd}" dev="${r0.dev}"`);
  await evalJs(`document.querySelector('details').open = true; document.getElementById('sync').click()`); await sleep(2000);
  const b1 = await backend();
  check('sync changes the backend clock', b1.timeSource !== 'millis' && b1.time !== b0.time, `before=${b0.time}/${b0.timeSource} after=${b1.time}/${b1.timeSource}`);
  check('sync feedback names old and new time', /was .* now/.test(await text('toast')), `toast="${await text('toast')}"`);
  check('device clock chip shows synced time', (await text('devtime')).includes(b1.time.slice(0, 5)), `chip="${await text('devtime')}"`);
  check('countdown becomes meaningful after sync', /in \d/.test(await text('countdown')), await text('countdown'));
} else console.log('skip: backend already synced');

// ---- 2. pick a new time (no save) ----
const before = await evalJs(`({cd: document.getElementById('countdown').textContent, time: document.getElementById('time').value})`);
const [h, m] = before.time.split(':').map(Number);
const newTime = String((h + 1) % 24).padStart(2, '0') + ':' + String(m).padStart(2, '0');
await evalJs(`(() => { const t = document.getElementById('time'); t.value = '${newTime}'; t.dispatchEvent(new Event('input', {bubbles:true})); t.dispatchEvent(new Event('change', {bubbles:true})); })()`);
await sleep(300);
const afterPick = await text('countdown');
check('countdown previews the new time before saving', afterPick !== before.cd && /would ring/.test(afterPick), `before="${before.cd}" after="${afterPick}"`);

// ---- 3. save ----
await evalJs(`document.getElementById('set').click()`); await sleep(1500);
const st = await backend();
check('backend settings.alarmTime updated', st.settings.alarmTime === newTime, `backend=${st.settings.alarmTime} expected=${newTime}`);
const afterSet = await evalJs(`({cd: document.getElementById('countdown').textContent, time: document.getElementById('time').value, toast: document.getElementById('toast').textContent})`);
check('UI countdown matches backend after save', afterSet.cd.includes(fmt(st.nextAlarmEpoch - st.now)) && !/would/.test(afterSet.cd), `ui="${afterSet.cd}" backend next=${st.nextAlarm}`);
check('save confirmation toast', afterSet.toast.includes(newTime), `toast="${afterSet.toast}"`);
check('time picker keeps the new value after save', afterSet.time === newTime, afterSet.time);
await sleep(5500);
const afterPoll = await evalJs(`({cd: document.getElementById('countdown').textContent, time: document.getElementById('time').value})`);
check('value survives the 5 s poll', afterPoll.time === newTime && !/would/.test(afterPoll.cd), JSON.stringify(afterPoll));

// ---- 4. tune style: box appears only for "new"/"surprise", text round-trips through the backend ----
await evalJs(`document.querySelector('.tune[data-id="builtin:siren_stairs"]')?.click()`); await sleep(200);
const hiddenForBuiltin = await evalJs(`!document.getElementById('stylebox').classList.contains('show')`);
await evalJs(`document.querySelector('.tune[data-id="new"]').click()`); await sleep(200);
const shownForNew = await evalJs(`document.getElementById('stylebox').classList.contains('show')`);
check('style box only shows for a Gemini melody', hiddenForBuiltin && shownForNew, `builtin=${hiddenForBuiltin ? 'hidden' : 'shown'} new=${shownForNew ? 'shown' : 'hidden'}`);
const style = 'spooky 8-bit ' + Date.now() % 1000;
await evalJs(`(() => { const i = document.getElementById('style'); i.value = ${JSON.stringify(style)}; i.dispatchEvent(new Event('input', {bubbles:true})); })()`);
await evalJs(`document.getElementById('set').click()`); await sleep(1500);
const st4 = await backend();
check('backend settings.tuneStyle saved', st4.settings.tuneStyle === style, `backend="${st4.settings.tuneStyle}" expected="${style}"`);
await sleep(5500);
check('style input survives the 5 s poll', (await evalJs(`document.getElementById('style').value`)) === style, await evalJs(`document.getElementById('style').value`));

console.log('console/errors:', logs.length ? logs : 'none');
ws.close();
process.exit(results.every(Boolean) ? 0 : 1);
