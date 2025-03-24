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
  "abcd1234ef56": { lat: 37.7749, lon: -122.4194 },
  "7890ghij12kl": { lat: 34.0522, lon: -118.2437 },
  "mnop3456qrst": { lat: 40.7128, lon: -74.0060 }
  // Add more as needed
};

// Store sensor data with last update timestamp
let sensorData = {};

// Initialize sensorData with all known sensors
Object.keys(sensorLocations).forEach(sensorId => {
  sensorData[sensorId] = {
    sensor_id: sensorId,
    temperature: null,
    mq9: null,
    mq135: null,
    lat: sensorLocations[sensorId].lat,
    lon: sensorLocations[sensorId].lon,
    lastUpdate: null // Null until data received
  };
});

// Handle POST requests from receiver node
app.post('/update', (req, res) => {
  const data = req.body;
  const sensorId = data.sensor_id;

  // Only update if sensor is known
  if (sensorLocations[sensorId]) {
    const enrichedData = {
      ...data,
      lat: sensorLocations[sensorId].lat,
      lon: sensorLocations[sensorId].lon,
      lastUpdate: Date.now() // Timestamp in milliseconds
    };
    sensorData[sensorId] = enrichedData;
    console.log('Received and enriched data:', enrichedData);
    io.emit('sensorUpdate', enrichedData);
  }
  res.sendStatus(200);
});

// Serve static files
app.use(express.static('public'));

// Send initial sensor data to new clients
io.on('connection', (socket) => {
  console.log('New client connected');
  socket.emit('initialSensors', Object.values(sensorData));
});

const PORT = 3000;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on port ${PORT}`);
});
