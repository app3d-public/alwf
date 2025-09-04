document.addEventListener("DOMContentLoaded", () => {
  const input   = document.getElementById("api-input");
  const sendBtn = document.getElementById("api-send");
  const form    = document.getElementById("send-form");
  const log     = document.getElementById("api-log");

  if (!input || !sendBtn || !form || !log) {
    console.warn("API Demo elements not found");
    return;
  }

  input.addEventListener("input", () => {
    sendBtn.disabled = !input.value.trim();
  });

  form.addEventListener("submit", e => {
    e.preventDefault();
    const msg = input.value.trim();
    if (!msg) return;

    alwf.emit("api-demo", msg);
    appendLog("REQUEST", msg);

    input.value = "";
    sendBtn.disabled = true;
  });

  alwf.on("api-demo", response => {
    appendLog("RESPONSE", JSON.stringify(response, null, 2));
  });

  function appendLog(type, text) {
    const item = document.createElement("div");
    item.className = "log-item " + (type === "RESPONSE" ? "ok" : "");
    item.innerHTML = `
      <div class="log-meta">
        <div class="badge">${type}</div>
        <time>${new Date().toLocaleTimeString()}</time>
      </div>
      <pre>${text}</pre>`;
    log.prepend(item);
  }
});