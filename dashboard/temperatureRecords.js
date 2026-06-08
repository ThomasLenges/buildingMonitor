// ───────────────────────── TEMPERATURE HISTORY ─────────────────────────
const tempHistory = JSON.parse(localStorage.getItem("tempHistory") || "[]").map(e => ({
    message:   e.message,
    timestamp: new Date(e.timestamp)  // convert string back to Date object
}));   // { message: string, timestamp: Date }

// ───────────────────────── CHART SETUP ─────────────────────────
const ctx = document.getElementById("temp-chart").getContext("2d"); // 2D drawing (webgl for 3D rendering)
const tempChart = new Chart(ctx, {
    type: "line",
    data: {
        labels: [], // timestamps (displayed on x axis)
        datasets: [{
            label:                "Temperature (°C)", 
            data:                 [], // values (displayed on y axis)
            borderColor:          "#38bdf8",
            borderWidth:          2,
            backgroundColor:      "#38bdf811",
            fill:                 true,
            tension:              0.4, // smooth curve
            pointRadius:          3,
            pointBackgroundColor: "#38bdf8",
        }]
    },
    options: {
        responsive: false, // use css sizing
        animation:  false, // skip animation on every update for performance
        scales: {
            x: {
                ticks: { color: "#475569", font: { family: "Courier New" } },
                grid:  { color: "#1e2530" }
            },
            y: {
                min:   25,
                max:   40,
                ticks: { color: "#475569", font: { family: "Courier New" } },
                grid:  { color: "#1e2530" }
            }
        },
        plugins: {
            legend:  { display: false }, // clean, no legend needed
            tooltip: {
                backgroundColor: "#111318",
                borderColor:     "#1e2530",
                borderWidth:     1,
                titleColor:      "#475569",
                bodyColor:       "#38bdf8",
                titleFont:       { family: "Courier New" },
                bodyFont:        { family: "Courier New" },
            }
        }
    }
});
// To show restored data (KEY LINE TO PROPERLY WORK)
renderChart();

// ───────────────────────── UPDATE RECORDS (called from hiveMQConnection.js) ─────────────────────────
function updateTemperature(message, timestamp) {
    document.getElementById("temp-value-r4").innerText      = message;
    document.getElementById("dot-temp-r4").className        = "temp-dot connected";
    document.getElementById("temp-current-value").innerText = message;

    tempHistory.push({ message, timestamp });

    saveHistory();
    renderChart();
}

// ───────────────────────── CHART RENDER ─────────────────────────
function renderChart() {
    const activeBtn = document.querySelector(".filter-btn.active");
    const range     = activeBtn ? activeBtn.dataset.range : "5m";
    const now       = new Date();

    const ranges = { "5m": 5*60*1000, "1h": 60*60*1000, "1d": 24*60*60*1000, "1w": 7*24*60*60*1000 };
    const cutoff = new Date(now - ranges[range]);

    const filtered = tempHistory.filter(e => e.timestamp >= cutoff);

    tempChart.data.labels           = filtered.map(e => e.timestamp.toLocaleTimeString("en-GB"));
    tempChart.data.datasets[0].data = filtered.map(e => parseFloat(e.message));
    tempChart.update();
}

// ───────────────────────── FILTER BUTTONS ─────────────────────────
document.querySelectorAll(".filter-btn").forEach(btn => {
    btn.addEventListener("click", () => {
        document.querySelectorAll(".filter-btn").forEach(b => b.classList.remove("active"));
        btn.classList.add("active");
        renderChart();
    });
});

// ───────────────────────── CLEAR HISTORY BUTTON ─────────────────────────
document.getElementById("clear-btn").addEventListener("click", () => {
    tempHistory.length = 0;                        // empties the array in place
    localStorage.removeItem("tempHistory");        // wipes localStorage
    renderChart();                                 // redraws empty chart
});

// ───────────────────────── SAVE HISTORY (One week max) ─────────────────────────
function saveHistory() {
    const oneWeekAgo = new Date(Date.now() - 7*24*60*60*1000);
    const pruned     = tempHistory.filter(e => e.timestamp >= oneWeekAgo);
    localStorage.setItem("tempHistory", JSON.stringify(pruned));
}