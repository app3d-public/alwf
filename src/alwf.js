// alwf.js
(function (global) {
  const listeners = new Map(); // name -> Set<fn>
  let transport = null;

  function tryParse(x) { try { return typeof x === 'string' ? JSON.parse(x) : x; } catch { return x; } }
  function serialize(x) { return typeof x === 'string' ? x : JSON.stringify(x); }
  function keyOf(msg) { return msg && typeof msg === 'object' ? (msg.handler ?? msg.event ?? msg.name) : undefined; }

  function dispatch(raw) {
    const data = (raw && raw.data !== undefined) ? raw.data : raw;
    const msg = tryParse(data);
    const k = keyOf(msg);
    if (!k) return;
    const set = listeners.get(k);
    if (set) for (const fn of [...set]) { try { fn(msg); } catch { } }
  }

  (function detect() {
    if (global.chrome?.webview?.postMessage) {
      transport = (m) => { global.chrome.webview.postMessage(m); return true; };
      global.chrome.webview.addEventListener('message', dispatch);
      return;
    }
    if (global.webkit?.messageHandlers?.handler?.postMessage) {
      transport = (m) => { global.webkit.messageHandlers.handler.postMessage(serialize(m)); return true; };
      const prev = global.__alwf_receive;
      global.__alwf_receive = function (payload) { dispatch(payload); if (typeof prev === 'function') try { prev(payload); } catch { } };
      return;
    }
    transport = () => { console.warn('alwf: no native bridge found'); return false; };
  })();

  // public API
  function ready() { return Promise.resolve(!!transport); }

  function emit(name, payload) {
    const base = { handler: name, event: name, name };
    const msg = (payload && typeof payload === 'object') ? { ...base, ...payload }
      : { ...base, message: payload };
    return transport ? transport(msg) : false;
  }

  function on(name, fn) {
    const k = String(name);
    if (!listeners.has(k)) listeners.set(k, new Set());
    const set = listeners.get(k);
    set.add(fn);
    return () => { set.delete(fn); if (!set.size) listeners.delete(k); };
  }

  function once(name, fn) {
    const off = on(name, (m) => { try { fn(m); } finally { off(); } });
    return off;
  }

  function off(name, fn) {
    const set = listeners.get(String(name));
    if (set) { set.delete(fn); if (!set.size) listeners.delete(name); }
  }

  global.alwf = { ready, emit, on, once, off };
})(window);
