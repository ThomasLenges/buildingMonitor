// ───────────────────────── DEVICE STATUS UPDATE ─────────────────────────
function updateDevice(id, status) {
    const card     = document.getElementById(`card-${id}`);
    const dot      = document.getElementById(`dot-${id}`);
    const statusEl = document.getElementById(`status-${id}`);
    const timeEl   = document.getElementById(`time-${id}`);

    const isConnected = status === "connected";

    card.className     = "device-card "   + (isConnected ? "connected" : "disconnected");
    dot.className      = "device-dot "    + (isConnected ? "connected" : "disconnected");
    statusEl.className = "device-status " + (isConnected ? "connected" : "disconnected");
    statusEl.innerText = status;
    timeEl.innerText   = new Date().toLocaleTimeString("en-GB");

    // Turn sensor connections to red if disconnected
    if (id === "r4" && !isConnected) {
        document.getElementById("dot-temp-r4").className = "temp-dot disconnected";
    }
}