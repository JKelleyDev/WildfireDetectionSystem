document.addEventListener('DOMContentLoaded', () => {
  const map = L.map('map').setView([37.7757, -117.0715], 4);
  
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
  }).addTo(map);

  const markers = {};
  const sensorData = {}; 

  const socket = io('http://143.110.193.195:80');

  socket.on('sensorUpdate', (data) => {
    const { sensor_id, temperature, mq9, mq135, lat, lon, fireRisk, connected } = data;
    
    const status = connected
      ? `Temp: ${temperature}°C<br>MQ9: ${mq9}<br>MQ135: ${mq135}<br>Risk: ${fireRisk}`
      : 'Disconnected';
    
    const color = connected
      ? (fireRisk === "High" ? "red" : "green")
      : "gray";

    if (markers[sensor_id]) {
      markers[sensor_id]
        .setLatLng([lat, lon])
        .setPopupContent(`Sensor: ${sensor_id}<br>${status}`)
        .setStyle({ color });
    } else {
      markers[sensor_id] = L.circle([lat, lon], {
        radius: 2000,
        color: color,
        fillOpacity: 0.5
      }).addTo(map).bindPopup(`Sensor: ${sensor_id}<br>${status}`);
    }

    // Save sensor info to display in table
    sensorData[sensor_id] = {
      temperature,
      mq9,
      mq135,
      fireRisk,
      connected,
      lastUpdated: new Date().toLocaleString()
    };

     updateSensorTable(); // Refresh the sensor table after every update
  });

  function updateSensorTable() {
    const container = document.getElementById('live-sensor-data');
    if (!container) return; // Make sure the div exists

    let html = `
      <table border="1" cellpadding="8" cellspacing="0">
        <thead>
          <tr>
            <th>Sensor ID</th>
            <th>Temperature (°C)</th>
            <th>MQ9</th>
            <th>MQ135</th>
            <th>Fire Risk</th>
            <th>Status</th>
            <th>Last Updated</th>
          </tr>
        </thead>
        <tbody>
    `;

    for (const [sensorId, info] of Object.entries(sensorData)) {
      html += `
        <tr>
          <td>${sensorId}</td>
          <td>${info.temperature}</td>
          <td>${info.mq9}</td>
          <td>${info.mq135}</td>
          <td>${info.fireRisk}</td>
          <td style="color: ${info.connected ? 'green' : 'gray'};">${info.connected ? 'Connected' : 'Disconnected'}</td>
          <td>${info.lastUpdated}</td>
        </tr>
      `;
    }

    html += '</tbody></table>';

    container.innerHTML = html;
  }
  
});
