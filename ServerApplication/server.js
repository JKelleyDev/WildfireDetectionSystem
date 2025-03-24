const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const cors = require('cors');

const app = express();
const server = http.createServer(app);
const io = socketIo(server, { cors: { origin: "*" } });

app.use(express.json());
app.use(cors());

// Lookup table for sensor chip IDs and their GPS coordinates
const sensorLocations = {
  "abcd1234ef56": { lat: 37.7749, lon: -122.4194 }, // San Francisco
  "7890ghij12kl": { lat: 34.0522, lon: -118.2437 }, // Los Angeles
  "mnop3456qrst": { lat: 40.7128, lon: -74.0060 }   // New York
  // Add more sensor chip IDs here
};

// Initialize sensor data with default values
let sensorData = {};
Object.keys(sensorLocations).forEach(sensorId => {
  sensorData[sensorId] = {
    sensor_id: sensorId,
    temperature: 0,  // Placeholder value
    mq9: 0,          // Placeholder value
    mq135: 0,        // Placeholder value
    lat: sensorLocations[sensorId].lat,
    lon: sensorLocations[sensorId].lon
  };
});

// Handle POST requests from receiver node (for when sensors are online)
app.post('/update', (req, res) => {
  const data = req.body;
  const sensorId = data.sensor_id;
  const location = sensorLocations[sensorId] || { lat: 0, lon: 0 };
  const enrichedData = { ...data, lat: location.lat, lon: location.lon };
  sensorData[sensorId] = enrichedData;
  console.log('Received and enriched data:', enrichedData);
  io.emit('sensorUpdate', enrichedData);
  res.sendStatus(200);
});

// Serve static files
app.use(express.static('public'));

// Send initial sensor data to new clients
io.on('connection', (socket) => {
  console.log('Client connected');
  Object.values(sensorData).forEach(data => {
    socket.emit('sensorUpdate', data); // Send all preloaded sensors
  });
});

// Start server
const PORT = 3000;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on port ${PORT}`);
});
