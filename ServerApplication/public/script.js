// Initialize Leaflet map
const map = L.map('map').setView([37.7749, -122.4194], 5); // Wider default view
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
  maxZoom: 19,
}).addTo(map);

const markers = {};

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
const socket = io('http://YOUR_DROPLET_IP:3000'); // Replace with Droplet IP

socket.on('sensorUpdate', (data) => {
  const { sensor_id, temperature, mq9, mq135, lat, lon } = data;

  // Update map
  if (markers[sensor_id]) {
    markers[sensor_id].setLatLng([lat, lon]);
    markers[sensor_id].setPopupContent(`Sensor: ${sensor_id}<br>Temp: ${temperature}°C<br>MQ9: ${mq9}<br>MQ135: ${mq135}`);
  } else {
    markers[sensor_id] = L.marker([lat, lon])
      .addTo(map)
      .bindPopup(`Sensor: ${sensor_id}<br>Temp: ${temperature}°C<br>MQ9: ${mq9}<br>MQ135: ${mq135}`);
  }

  // Update chart (example: show data for the first sensor or a selected one)
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
