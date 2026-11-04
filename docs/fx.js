// PixiJS backdrop: catalog grooveScene, full-viewport, no CDN.
const C = { hkrMagenta: 0xff00ff, hkrCyan: 0x00e0e0 };
const BPM = 124;
const BEAT = 60 / BPM;
const SIXTEENTH = BEAT / 4;
const reduceMotion = matchMedia('(prefers-reduced-motion: reduce)').matches;

const pointer = { x: 0.5, y: 0.5 };
addEventListener('pointermove', (e) => {
  pointer.x = e.clientX / innerWidth;
  pointer.y = e.clientY / innerHeight;
}, { passive: true });

start().catch((err) => {
  console.info('fx: animated backdrop disabled:', err?.message || err);
  document.getElementById('fx')?.remove();
});

async function start() {
  const PIXI = await import('./vendor/pixi.min.mjs');
  const app = new PIXI.Application();
  await app.init({
    canvas: document.getElementById('fx'),
    resizeTo: window,
    backgroundAlpha: 0,
    antialias: true,
    autoDensity: true,
    resolution: Math.min(devicePixelRatio || 1, 2),
    preference: 'webgl',
  });

  const scene = grooveScene(PIXI);
  app.stage.addChild(scene.root);
  bootTicker(app, scene);
}

function bootTicker(app, scene) {
  if (reduceMotion) {
    app.ticker.stop();
    const still = () => {
      const { width: w, height: h } = app.screen;
      scene.update(8, w, h, 0);
      app.render();
    };
    addEventListener('resize', () => requestAnimationFrame(still));
    still();
    return;
  }

  let t = 0;
  app.ticker.add((ticker) => {
    const dt = Math.min(ticker.deltaMS / 1000, 0.05);
    t += dt;
    const { width: w, height: h } = app.screen;
    const ease = Math.min(1, dt * 2.5);
    scene.root.x += ((0.5 - pointer.x) * 40 - scene.root.x) * ease;
    scene.root.y += ((0.5 - pointer.y) * 24 - scene.root.y) * ease;
    scene.update(t, w, h, dt);
  });

  document.addEventListener('visibilitychange', () => {
    if (document.hidden) app.ticker.stop(); else app.ticker.start();
  });
}

function grooveScene(PIXI) {
  const root = new PIXI.Container();
  const g = new PIXI.Graphics();
  g.blendMode = 'add';
  root.addChild(g);

  const pattern = [
    'x.x.x.x.x.x.x.x.',
    '..x...x...x...x.',
    'x...............',
    '....x.......x...',
    '.x.x.x.x.x.x.x.x',
    'x..x..x...x..x..',
    '...x.....x..x...',
    '....x.......x..x',
  ].map((row) => [...row].map((c) => c === 'x'));
  const LANES = pattern.length, STEPS = 16;
  const swing = [0, 0.12, 0, 0.2, 0.08, 0.16, 0.24, 0.1];
  const flash = pattern.map(() => new Float32Array(STEPS));
  const lastK = new Array(LANES).fill(-1);

  return {
    root,
    update(t, w, h, dt) {
      for (let r = 0; r < LANES; r++) {
        let k = Math.floor(t / SIXTEENTH);
        if (k % 2 === 1 && t < (k + swing[r]) * SIXTEENTH) k -= 1;
        if (k !== lastK[r]) {
          lastK[r] = k;
          const s = k % STEPS;
          if (pattern[r][s]) flash[r][s] = 1;
        }
        for (let s = 0; s < STEPS; s++) flash[r][s] *= Math.exp(-dt * 4);
      }

      g.clear();
      const hy = h * 0.42, cx = w * (0.5 + 0.04 * Math.sin(t * 0.21));
      const cellW = Math.max(w, 700) * 0.075, play = Math.floor(t / SIXTEENTH) % STEPS;
      const P = (col, z, side) => [cx + (col - STEPS / 2) * cellW / z, hy + side * (h * 0.7) / z];

      for (const side of [1, -1]) {
        const dim = side === 1 ? 1 : 0.45;
        for (let r = 0; r < LANES; r++) {
          const z0 = 1 + r * 0.55, z1 = z0 + 0.45;
          for (let s = 0; s < STEPS; s++) {
            const a = P(s + 0.06, z0, side), b = P(s + 0.94, z0, side);
            const c = P(s + 0.94, z1, side), d = P(s + 0.06, z1, side);
            const quad = [...a, ...b, ...c, ...d];
            const color = s % 4 === 0 ? C.hkrMagenta : C.hkrCyan;
            const f = flash[r][s];
            const onHead = s === play ? 0.12 : 0;
            g.poly(quad)
              .fill({ color, alpha: (0.06 + onHead + 0.8 * f) * dim / (1 + r * 0.15) })
              .stroke({ width: 1.5, color, alpha: (0.25 + 0.5 * f) * dim });
          }
        }
      }
    },
  };
}
