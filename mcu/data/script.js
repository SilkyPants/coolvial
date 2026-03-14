let tChart;
let lastLogTimestamp = 0;
let allDataRows = []; // Stores all log entries
let currentPage = 1;
const rowsPerPage = 15;

async function init() {
  try {
    const res = await fetch("/api/logs");
    const csv = await res.text();
    const lines = csv.trim().split("\n");

    allDataRows = [];
    const chartLabels = [];
    const chartData = [];
    const chartInternalData = [];
    const chartAmbientData = [];

    // Parse CSV into array
    lines.forEach((line) => {
      const [ts, temp, internalTemp, ambientTemp] = line.split(",");
      if (!ts) return;
      allDataRows.push({
        ts: parseInt(ts),
        temp: parseFloat(temp),
        internalTemp: parseFloat(internalTemp),
        ambientTemp: parseFloat(ambientTemp),
        timeStr: new Date(ts * 1000).toLocaleString(),
      });
    });

    // Sort: Newest timestamp first
    allDataRows.sort((a, b) => b.ts - a.ts);

    if (allDataRows.length > 0) {
      lastLogTimestamp = allDataRows[0].ts;

      document.getElementById("lastSyncTime").innerText = new Date(
        lastLogTimestamp * 1000,
      ).toLocaleTimeString();

      // Build Chart Data (use last 30 points, chronological order)
      const chartSlice = [...allDataRows].slice(0, 30).reverse();
      chartSlice.forEach((row) => {
        chartLabels.push(new Date(row.ts * 1000).toLocaleTimeString());
        chartData.push(row.temp);
        chartInternalData.push(row.internalTemp);
        chartAmbientData.push(row.ambientTemp);
      });
    }

    setupChart(chartLabels, chartData, chartInternalData, chartAmbientData);
    renderTable();
  } catch (e) {
    console.error("Load failed", e);
  }
}

function renderTable() {
  const tbody = document.getElementById("logBody");
  tbody.innerHTML = "";

  const start = (currentPage - 1) * rowsPerPage;
  const end = start + rowsPerPage;
  const pageRows = allDataRows.slice(start, end);

  pageRows.forEach((row) => {
    const tr = tbody.insertRow();
    tr.innerHTML = `<td>${row.timeStr}</td><td>${row.temp.toFixed(2)} °C</td><td>${row.internalTemp.toFixed(2)} °C</td><td>${row.ambientTemp.toFixed(2)} °C</td>`;
  });

  // Update Pagination UI
  document.getElementById("pageInfo").innerText =
    `Page ${currentPage} of ${Math.ceil(allDataRows.length / rowsPerPage) || 1}`;
  document.getElementById("prevBtn").disabled = currentPage === 1;
  document.getElementById("nextBtn").disabled = end >= allDataRows.length;
}

function changePage(step) {
  currentPage += step;
  renderTable();
}

async function updateLive() {
  const dot = document.getElementById("status");
  
  const curTempElem = document.getElementById("curBlkTemp");
  const avgTempElem = document.getElementById("avgBlkTemp");

  const curInternalTempElem = document.getElementById("curIntTemp");
  const avgInternalTempElem = document.getElementById("avgIntTemp");

  const curAmbientTempElem = document.getElementById("curAmbTemp");
  const avgAmbientTempElem = document.getElementById("avgAmbTemp");

  try {
    const res = await fetch("/api/data");
    if (!res.ok) throw new Error("Server error");
    const info = await res.json();

    // Pulse effect
    if (dot) {
      dot.classList.remove("sync-flash");
      void dot.offsetWidth; // This "magic" line forces a CSS reflow to restart the animation
      dot.classList.add("sync-flash");
      dot.className = "status-dot status-online sync-flash";
    }

    if (curTempElem) curTempElem.innerText = info.current.block.toFixed(2) + " °C";
    if (avgTempElem) avgTempElem.innerText = info.average.block.toFixed(2) + " °C";

    if (curInternalTempElem) curInternalTempElem.innerText = info.current.internal.toFixed(2) + " °C";
    if (avgInternalTempElem) avgInternalTempElem.innerText = info.average.internal.toFixed(2) + " °C";

    if (curAmbientTempElem) curAmbientTempElem.innerText = info.current.ambient.toFixed(2) + " °C";
    if (avgAmbientTempElem) avgAmbientTempElem.innerText = info.average.ambient.toFixed(2) + " °C";

    // OPTION B: Only update if the timestamp is newer
    // Safety: Ensure allDataRows exists and we have a valid timestamp
    if (info.timestamp > lastLogTimestamp && Array.isArray(allDataRows)) {
      console.log("New log detected via API!");
      lastLogTimestamp = info.timestamp;

      const timeObj = new Date(info.timestamp * 1000);
      const timeStr = timeObj.toLocaleString();

      allDataRows.unshift({
        ts: info.timestamp,
        temp: info.current.block,
        internalTemp: info.current.internal,
        ambientTemp: info.current.ambient,
        timeStr: timeStr,
      });

      if (currentPage === 1) renderTable();

      const syncElem = document.getElementById("lastSyncTime");
      if (syncElem) syncElem.innerText = timeObj.toLocaleTimeString();

      updateChartRange();
    }
  } catch (e) {
    console.error("AJAX Error:", e); // This tells you exactly what went wrong in F12
    if (dot) {
      dot.className = "status-dot status-offline";
      dot.classList.remove("sync-flash");
    }
  }
}

function setupChart(l, blockData, internalData, ambientData) {
  const ctx = document.getElementById("tChart").getContext("2d");
  tChart = new Chart(ctx, {
    type: "line",
    data: {
      labels: l,
      datasets: [
        {
          label: "Temp (°C)",
          data: blockData,
          borderColor: "#4bc0c0",
          backgroundColor: "rgba(75, 192, 192, 0.1)",
          fill: true,
          tension: 0.3,
        },
        {
          label: "Internal Temp (°C)",
          data: internalData,
          borderColor: "#36d465",
          backgroundColor: "rgba(75, 192, 192, 0.1)",
          fill: true,
          tension: 0.3,
        },
        {
          label: "Ambient Temp (°C)",
          data: ambientData,
          borderColor: "#a841e4",
          backgroundColor: "rgba(75, 192, 192, 0.1)",
          fill: true,
          tension: 0.3,
        },
      ],
    },
    options: {
      maintainAspectRatio: false,
      scales: {
        y: {
          type: "linear",
          suggestedMin: 10,
          suggestedMax: 30,
          beginAtZero: false,
        },
      },
    },
  });
}

function updateChartRange() {
  if (!tChart || !allDataRows.length) return;

  const range = document.querySelector('input[name="range"]:checked').value;
  const now = Math.floor(Date.now() / 1000);
  let secondsLimit = 3600; // 1 Hour default
  let sampleRate = 1; // Show every point

  if (range === "day") {
    secondsLimit = 86400;
    sampleRate = 3; // Show every 3rd point (~15 min intervals)
  } else if (range === "week") {
    secondsLimit = 604800;
    sampleRate = 12; // Show every 12th point (~1 hour intervals)
  }

  const filtered = allDataRows
    .filter((row) => row.ts > now - secondsLimit)
    .reverse();

  const labels = [];
  const data = [];
  const internalData = [];
  const ambientData = [];

  filtered.forEach((row, index) => {
    if (index % sampleRate === 0) {
      const date = new Date(row.ts * 1000);
      // Format label based on range
      const label =
        range === "hour"
          ? date.toLocaleTimeString()
          : date.toLocaleDateString() + " " + date.getHours() + ":00";
      labels.push(label);
      data.push(row.temp);
      internalData.push(row.internalTemp);
      ambientData.push(row.ambientTemp);
    }
  });

  tChart.data.labels = labels;
  tChart.data.datasets[0].data = data;
  tChart.data.datasets[1].data = internalData;
  tChart.data.datasets[2].data = ambientData;

  // Adjust Y-Axis for the specific range found in this data
  tChart.options.scales.y.min = Math.floor(Math.min(...data) - 2);
  tChart.options.scales.y.max = Math.ceil(Math.max(...data) + 2);

  tChart.update();
}

function toggleMode() {
  const isDark = document.body.classList.toggle("dark-mode");
  document.body.classList.remove("light-mode");
  if (!isDark) document.body.classList.add("light-mode");
  localStorage.setItem("theme", isDark ? "dark" : "light");
}

function downloadCSV() {
  // Build from the internal data array, not the HTML elements
  let csv = "Timestamp,Date,Block Temp,Internal Temp,Ambient Temp\n";
  allDataRows.forEach(row => {
    csv += `${row.ts},${row.timeStr},${row.temp},${row.internalTemp},${row.ambientTemp}\n`;
  });
  
  const blob = new Blob([csv], { type: "text/csv" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `coolvial_logs_${Date.now()}.csv`;
  a.click();
}

if (localStorage.getItem("theme") === "dark")
  document.body.classList.add("dark-mode");
window.onload = init;
setInterval(updateLive, 5000);
