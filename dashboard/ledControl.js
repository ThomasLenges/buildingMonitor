// ───────────────────────── CANVAS REFERENCES ─────────────────────────
const pickerCanvas = document.getElementById("led-picker");
const hueCanvas    = document.getElementById("led-hue");
const pickerCtx    = pickerCanvas.getContext("2d");
const hueCtx       = hueCanvas.getContext("2d");

// ───────────────────────── STATE ─────────────────────────
let currentHue = 0;       // 0-360
let currentHex = "000000";
let ledOn      = false;
let lastColor  = "FF0000"; // restored when turning back on

// ───────────────────────── DRAW HUE SLIDER ─────────────────────────
function drawHueSlider() {
    const gradient = hueCtx.createLinearGradient(0, 0, hueCanvas.width, 0);  // Gradient from x1 to x2 (no y change)
    gradient.addColorStop(0,    "hsl(0,   100%, 50%)"); // Hue Saturation Lightness (100% Saturation: Vivid color (0% is gray) 50% Lightness: Pure color (0% is black and 100% is white))
    gradient.addColorStop(0.17, "hsl(60,  100%, 50%)");
    gradient.addColorStop(0.33, "hsl(120, 100%, 50%)");
    gradient.addColorStop(0.5,  "hsl(180, 100%, 50%)");
    gradient.addColorStop(0.67, "hsl(240, 100%, 50%)");
    gradient.addColorStop(0.83, "hsl(300, 100%, 50%)");
    gradient.addColorStop(1,    "hsl(360, 100%, 50%)");

    hueCtx.fillStyle = gradient; // Paint with generated gradient
    hueCtx.fillRect(0, 0, hueCanvas.width, hueCanvas.height); // Fill entire led-hue canva
}

// ───────────────────────── DRAW 2D PICKER ─────────────────────────
function drawPicker() {
    const w = pickerCanvas.width;
    const h = pickerCanvas.height;

    // Base hue color left to right (white → hue)
    const hueGradient = pickerCtx.createLinearGradient(0, 0, w, 0);
    hueGradient.addColorStop(0, "white");
    hueGradient.addColorStop(1, `hsl(${currentHue}, 100%, 50%)`);
    pickerCtx.fillStyle = hueGradient;
    pickerCtx.fillRect(0, 0, w, h);

    // Overlay top to bottom (transparent → black)
    const darkGradient = pickerCtx.createLinearGradient(0, 0, 0, h);
    darkGradient.addColorStop(0, "rgba(0,0,0,0)");
    darkGradient.addColorStop(1, "rgba(0,0,0,1)");
    pickerCtx.fillStyle = darkGradient;
    pickerCtx.fillRect(0, 0, w, h);
}

// ───────────────────────── RESIZE CANVASES ─────────────────────────
function resizeCanvases() {
    pickerCanvas.width  = pickerCanvas.offsetWidth;
    pickerCanvas.height = pickerCanvas.offsetHeight;
    hueCanvas.width     = hueCanvas.offsetWidth;
    hueCanvas.height    = hueCanvas.offsetHeight;
    drawPicker();
    drawHueSlider();
}

// ───────────────────────── SAMPLE COLOR FROM PICKER ─────────────────────────
function samplePicker(x, y) {
    const pixel = pickerCtx.getImageData(x, y, 1, 1).data;
    return rgbToHex(pixel[0], pixel[1], pixel[2]);
}

function sampleHue(x) {
    const pixel = hueCtx.getImageData(x, 0, 1, 1).data;
    currentHue  = Math.round((x / hueCanvas.width) * 360);
    drawPicker();
}

// ───────────────────────── RGB → HEX ─────────────────────────
function rgbToHex(r, g, b) {
    return [r, g, b].map(v => v.toString(16).padStart(2, "0")).join("").toUpperCase();
}

// ───────────────────────── UPDATE UI + PUBLISH ─────────────────────────
function applyColor(hex) {
    currentHex = hex;
    lastColor  = hex;
    document.getElementById("led-preview").style.background = "#" + hex;
    document.getElementById("led-hex").innerText            = "#" + hex;
    if (ledOn) publishLED(hex);
}

function publishLED(hex) {
    client.publish("esp32/r4/led", hex, { retain: true });
}

// ───────────────────────── PICKER INTERACTION ─────────────────────────
let draggingPicker = false;
let draggingHue    = false;

pickerCanvas.addEventListener("mousedown", e => { draggingPicker = true;  handlePicker(e); });
pickerCanvas.addEventListener("mousemove", e => { if (draggingPicker) handlePicker(e); });
pickerCanvas.addEventListener("mouseup",   () => { draggingPicker = false; });

hueCanvas.addEventListener("mousedown", e => { draggingHue = true;  handleHue(e); });
hueCanvas.addEventListener("mousemove", e => { if (draggingHue) handleHue(e); });
hueCanvas.addEventListener("mouseup",   () => { draggingHue = false; });

function handlePicker(e) {
    const rect = pickerCanvas.getBoundingClientRect();
    const x    = Math.max(0, Math.min(e.clientX - rect.left, pickerCanvas.width  - 1));
    const y    = Math.max(0, Math.min(e.clientY - rect.top,  pickerCanvas.height - 1));
    applyColor(samplePicker(x, y));
}

function handleHue(e) {
    const rect = hueCanvas.getBoundingClientRect();
    const x    = Math.max(0, Math.min(e.clientX - rect.left, hueCanvas.width - 1));
    sampleHue(x);
    applyColor(samplePicker(
        Math.floor(pickerCanvas.width  / 2),
        Math.floor(pickerCanvas.height / 2)
    ));
}

// ───────────────────────── ON / OFF BUTTONS ─────────────────────────
document.getElementById("led-on").addEventListener("click", () => {
    ledOn = true;
    document.getElementById("led-on").classList.add("active");
    document.getElementById("led-off").classList.remove("active");
    publishLED(lastColor);
});

document.getElementById("led-off").addEventListener("click", () => {
    ledOn = false;
    document.getElementById("led-off").classList.add("active");
    document.getElementById("led-on").classList.remove("active");
    publishLED("000000");
});

// ───────────────────────── INIT ─────────────────────────
window.addEventListener("resize", resizeCanvases);
window.addEventListener("load",   resizeCanvases);