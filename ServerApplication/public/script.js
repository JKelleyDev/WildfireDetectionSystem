// Initialize Leaflet map
const map = L.map('map').setView([37.7749, -122.4194], 5);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
  maxZoom: 19,
}).addTo(map);

const markers = {};

// Define custom icons for sensor states
const grayIcon = L.divIcon({ className: 'sensor-icon', html: '<div style="background-color: gray; width: 20px; height: 20px; border-radius: 50%;"></div>' });
const greenIcon = L.divIcon({ className: 'sensor-icon', html: '<div style="background-color: green; width: 20px; height: 20px; border-radius: 50%;"></div>' });
const redIcon = L.divIcon({ className: 'sensor-icon', html: '<div style="background-color: red; width: 20px; height: 20px; border-radius: 50%;"></div>' });

// Initialize Chart.js
const ctx = document.getElementById('sensorChart').getContext('2d');
const chart = new Chart(ctx, {
  type: 'line',
  data: {
    labels: [],
    datasets: [
      { label: 'Temperature', data: [], borderColor: 'red', fill: false },
      { label: 'MQ9', data: [], borderColor: 'blue', fill: false },
      { label: 'MQ135', data: [], borderColor: 'green', fill: false }
    ]
  },
  options: { scales: { x: { type: 'time', time: { unit: 'second' } } } }
});

// Connect to WebSocket server
const socket = io('http://143.110.193.195:3000'); // Use your Droplet IP

// Function to determine fire risk
function isFireRisk(temperature, mq9, mq135) {
  return temperature > 30 || mq9 > 500; // Example thresholds; adjust as needed
}

// Function to update marker state
function updateMarker(sensor) {
  const { sensor_id, temperature, mq9, mq135, lat, lon, lastUpdate } = sensor;
  const now = Date.now();
  const oneHour = 60 * 60 * 1000; // 1 hour in milliseconds

  let icon;
  if (!lastUpdate || (now - lastUpdate > oneHour)) {
    icon = grayIcon; // No data or stale
  } else if (isFireRisk(temperature, mq9, mq135)) {
    icon = redIcon; // Fire risk
  } else {
    icon = greenIcon; // No fire risk
  }

  if (markers[sensor_id]) {
    markers[sensor_id].setLatLng([lat, lon]).setIcon(icon);
    markers[sensor_id].setPopupContent(
      `Sensor: ${sensor_id}<br>Temp: ${temperature ?? 'N/A'}°C<br>MQ9: ${mq9 ?? 'N/A'}<br>MQ135: ${mq135 ?? 'N/A'}`
    );
  } else {
    markers[sensor_id] = L.marker([lat, lon], { icon })
      .addTo(map)
      .bindPopup(
        `Sensor: ${sensor_id}<br>Temp: ${temperature ?? 'N/A'}°C<br>MQ9: ${mq9 ?? 'N/A'}<br>MQ135: ${mq135 ?? 'N/A'}`
      );
  }
}

// Handle initial sensor data
socket.on('initialSensors', (sensors) => {
  sensors.forEach(sensor => updateMarker(sensor));
});

// Handle sensor updates
socket.on('sensorUpdate', (data) => {
  updateMarker(data);

  // Update chart (example: first sensor or selected)
  const { sensor_id, temperature, mq9, mq135 } = data;
  if (Object.keys(markers).length === 1 || chart.data.datasets[0].label === sensor_id) {
    const time = new Date();
    chart.data.labels.push(time);
    chart.data.datasets[0].data.push(temperature);
    chart.data.datasets[1].data.push(mq9);
    chart.data.datasets[2].data.push(mq135);
    if (chart.data.labels.length > 20) {
      chart.data.labels.shift();
      chart.data.datasets.forEach(dataset => dataset.data.shift());
    }
    chart.update();
  }
});
