// Load current settings when the page opens
window.onload = async () => {
    if (localStorage.getItem("theme") === "dark") document.body.classList.add("dark-mode");
    
    try {
        const res = await fetch('/api/config');
        const data = await res.json();
        document.getElementById('setpoint').value = data.setpoint;
        document.getElementById('kp').value = data.kp;
        document.getElementById('ki').value = data.ki;
        document.getElementById('kd').value = data.kd;
    } catch (e) {
        console.error("Failed to load config", e);
    }
};

// Save PID Settings
async function saveConfig() {
    const payload = {
        setpoint: parseFloat(document.getElementById('setpoint').value),
        kp: parseFloat(document.getElementById('kp').value),
        ki: parseFloat(document.getElementById('ki').value),
        kd: parseFloat(document.getElementById('kd').value)
    };

    const statusEl = document.getElementById('saveStatus');
    statusEl.innerText = "Saving...";

    try {
        const res = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        if (res.ok) statusEl.innerText = "Saved successfully!";
        else statusEl.innerText = "Error saving settings.";
    } catch (e) {
        statusEl.innerText = "Network error.";
    }
    setTimeout(() => statusEl.innerText = "", 3000);
}

// Probe Assignment & Polling Logic
let pollInterval;

async function startAssignment(roleIndex) {
    const btn = document.querySelectorAll('.assign-btn')[roleIndex];
    const originalText = btn.innerText;
    btn.innerText = "Pinch Probe Now...";
    btn.disabled = true;

    await fetch('/api/assign/start', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: `roleIndex=${roleIndex}`
    });
    
    // Poll the ESP32 every 1 second
    pollInterval = setInterval(async () => {
        const res = await fetch('/api/assign/status');
        const data = await res.json();

        if (data.status === 'success') {
            clearInterval(pollInterval);
            btn.innerText = "Success ✓";
            btn.style.background = "#4caf50";
        } else if (data.status === 'failed') {
            clearInterval(pollInterval);
            btn.innerText = "Timeout ✗";
            btn.style.background = "#f44336";
        }

        // Reset button after a few seconds
        if (data.status !== 'searching') {
            setTimeout(() => {
                btn.innerText = originalText;
                btn.disabled = false;
                btn.style.background = "";
            }, 4000);
        }
    }, 1000);
}