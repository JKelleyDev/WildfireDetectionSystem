// Initialize Leaflet map
const map = L.map('map').setView([37.7749, -122.4194], 5);
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

// Function to update or create a marker
function updateMarker(data) {
  const { sensor_id, temperature, mq9, mq135, lat, lon } = data;
  const tempStr = temperature !== null ? `${temperature}°C` : 'N/A';
  const mq9Str = mq9 !== null ? mq9 : 'N/A';
  const mq135Str = mq135 !== null ? mq135 : 'N/A';

  if (markers[sensor_id]) {
    markers[sensor_id].setLatLng([lat, lon]);
    markers[sensor_id].setPopupContent(`Sensor: ${sensor_id}<br>Temp: ${tempStr}<br>MQ9: ${mq9Str}<br>MQ135: ${mq135Str}`);
  } else {
    markers[sensor_id] = L.marker([lat, lon])
      .addTo(map)
      .bindPopup(`Sensor: ${sensor_id}<br>Temp: ${tempStr}<br>MQ9: ${mq9Str}<br>MQ135: ${mq135Str}`);
  }
}

// Fetch initial sensor data
fetch('http://143.110.193.195:3000/sensors')
  .then(response => response.json())
  .then(sensors => {
    sensors.forEach(updateMarker);
  })
  .catch(error => console.error('Error fetching initial sensors:', error));

// Connect to WebSocket server
const socket = io('http://143.110.193.195:3000');

socket.on('sensorUpdate', (data) => {
  updateMarker(data);

  // Update chart (example: show data for the first sensor or a selected one)
  if (Object.keys(markers).length === 1 || chart.data.datasets[0].label === data.sensor_id) {
    const time = new Date();
    chart.data.labels.push(time);
    chart.data.datasets[0].data.push(data.temperature);
    chart.data.datasets[1].data.push(data.mq9);
    chart.data.datasets[2].data.push(data.mq135);
    if (chart.data.labels.length > 20) {
      chart.data.labels.shift();
      chart.data.datasets.forEach(dataset => dataset.data.shift());
    }
    chart.update();
  }
});
