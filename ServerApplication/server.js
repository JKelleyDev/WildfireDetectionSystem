const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = socketIo(server, { cors: { origin: "*" } });

app.use(express.json());
app.use(cors());

// Serve static files from 'public'
app.use(express.static('public'));
// Serve 'node_modules' for dependencies
app.use('/node_modules', express.static(path.join(__dirname, 'node_modules')));

// Sensor locations
const sensorLocations = {
  "abcd1234ef56": { lat: 37.7749, lon: -122.4194 }, // San Francisco
  "7890ghij12kl": { lat: 34.0522, lon: -118.2437 }, // Los Angeles
  "mnop3456qrst": { lat: 40.7128, lon: -74.0060 }   // New York
};

// Initialize sensor data
let sensorData = {};
Object.keys(sensorLocations).forEach(sensorId => {
  sensorData[sensorId] = {
    sensor_id: sensorId,
    temperature: 0,
    mq9: 0,
    mq135: 0,
    lat: sensorLocations[sensorId].lat,
    lon: sensorLocations[sensorId].lon,
    connected: false
  };
});

// Handle sensor updates
app.post('/update', (req, res) => {
  const data = req.body;
  const sensorId = data.sensor_id;
  const location = sensorLocations[sensorId] || { lat: 0, lon: 0 };
  const enrichedData = { ...data, lat: location.lat, lon: location.lon, connected: true };
  sensorData[sensorId] = enrichedData;
  console.log('Sensor update:', enrichedData);
  io.emit('sensorUpdate', enrichedData);
  res.sendStatus(200);
});

// Send initial data to clients
io.on('connection', (socket) => {
  console.log('Client connected');
  Object.values(sensorData).forEach(data => socket.emit('sensorUpdate', data));
});

const PORT = 3000;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on port ${PORT}`);
});
