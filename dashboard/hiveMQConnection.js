// ── HIVEMQ CREDENTIALS (in config.js) ────────────────────────────────────────
const MQTT_TOPICS = ["esp32/r4/status", "esp32/q/status"]; // subscribe to both device status
// ─────────────────────────────────────────────────────────────────

const brokerDot   = document.getElementById("broker-dot");
const brokerLabel = document.getElementById("broker-label");
const log         = document.getElementById("message-log");
const emptyState  = document.getElementById("empty-state");
const msgCount    = document.getElementById("msg-count");
const lastTime    = document.getElementById("last-time");

let count = 0;

// 8884 MQTT broker over wss browser
// 8883 ESP32 to MQTT broker
const client = mqtt.connect(`wss://${MQTT_HOST}:8884/mqtt`, { // wss: WebSocket Secure (connection stays open permanently both sides can send messages at any time in either direction unlike HTTP where browser asks and servers answers)
    username:  MQTT_USER,
    password:  MQTT_PASS,
    clientId:  "dashboard-" + Math.random().toString(16).slice(2, 8), // Browser client ID (unique to avoid conflicts) (6 random hex chars), slice to remove "0." at the start of the string returned by Math.random()
    reconnectPeriod: 3000, // MQTT.js tries to reconnect every 3 seconds if connection is lost
});

client.on("connect", () => {
    brokerDot.className   = "connected";
    brokerLabel.innerText = "broker connected";
    MQTT_TOPICS.forEach(topic => client.subscribe(topic)); // Subscribes to each topic in the MQTT_TOPICS array when connected
});

client.on("offline", () => {
    brokerDot.className   = "disconnected";
    brokerLabel.innerText = "broker offline";
});

client.on("reconnect", () => {
    brokerDot.className   = "connecting";
    brokerLabel.innerText = "reconnecting...";
});

client.on("message", (topic, payload) => { // receives topic and payload as parameters when a message is received
    const message = payload.toString();

    // Device cards
    if (topic === "esp32/r4/status") updateDevice("r4", message);
    if (topic === "esp32/q/status")  updateDevice("q", message);

    // Message log
    if (emptyState.parentNode) emptyState.remove(); // Remove the "No messages yet" text if it's still there

    count++;
    msgCount.innerText = count;

    const now = new Date();
    const timeStr = now.toLocaleTimeString("en-GB");
    lastTime.innerText = timeStr;

    const entry = document.createElement("div");
    entry.className = "log-entry";
    // Without template literals — "<span>" + timeStr + "</span>"
    // With template literals — `<span>${timeStr}</span>`
    entry.innerHTML = `
        <span class="log-time">${timeStr}</span> 
        <span class="log-topic">${topic}</span>
        <span class="log-payload">${payload.toString()}</span>
    `;

    log.appendChild(entry); // Adds the new log entry to the end of the message log
    log.scrollTop = log.scrollHeight; // Auto-scroll to the bottom of the log when a new message is added
});

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
}