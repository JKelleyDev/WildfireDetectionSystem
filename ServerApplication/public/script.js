document.addEventListener('DOMContentLoaded', () => {
  const map = L.map('map').setView([37.7749, -122.4194], 5);
  
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '© <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
  }).addTo(map);

  const markers = {};

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
      markers[sensor_id] = L.circleMarker([lat, lon], {
        radius: 10,
        color: color,
        fillOpacity: 0.5
      }).addTo(map).bindPopup(`Sensor: ${sensor_id}<br>${status}`);
    }
  });
});
