document.addEventListener('DOMContentLoaded', () => {
  const map = L.map('map').setView([37.7749, -122.4194], 5);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
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
    options: {
      scales: {
        x: { type: 'time', time: { unit: 'second' } }
      }
    }
  });

  const socket = io('http://143.110.193.195:80');

  socket.on('sensorUpdate', (data) => {
    const { sensor_id, temperature, mq9, mq135, lat, lon, fireRisk, connected } = data;
    const status = connected ? `Temp: ${temperature}°C<br>MQ9: ${mq9}<br>MQ135: ${mq135}<br>Risk: ${fireRisk}` : 'Disconnected';
    const color = connected ? (fireRisk === "High" ? "red" : "green") : "gray";

    if (markers[sensor_id]) {
      markers[sensor_id].setLatLng([lat, lon]).setPopupContent(`Sensor: ${sensor_id}<br>${status}`).setStyle({ color });
    } else {
      markers[sensor_id] = L.circleMarker([lat, lon], {
        radius: 10,
        color: color,
        fillOpacity: 0.5
      }).addTo(map).bindPopup(`Sensor: ${sensor_id}<br>${status}`);
    }

    if (connected) {
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
});
