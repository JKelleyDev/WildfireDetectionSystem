const map = L.map('map').setView([37.7749, -122.4194], 5);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
  maxZoom: 19,
}).addTo(map);

const markers = {};

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

const socket = io('http://143.110.193.195:3000');

socket.on('sensorUpdate', (data) => {
  const { sensor_id, temperature, mq9, mq135, lat, lon } = data;

  if (markers[sensor_id]) {
    markers[sensor_id].setLatLng([lat, lon]);
    markers[sensor_id].setPopupContent(`Sensor: ${sensor_id}<br>Temp: ${temperature}°C<br>MQ9: ${mq9}<br>MQ135: ${mq135}`);
  } else {
    markers[sensor_id] = L.marker([lat, lon])
      .addTo(map)
      .bindPopup(`Sensor: ${sensor_id}<br>Temp: ${temperature}°C<br>MQ9: ${mq9}<br>MQ135: ${mq135}`);
  }

  if (temperature !== 0 || mq9 !== 0 || mq135 !== 0) {
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
