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
  "abcd1234ef56": { lat: 37.7749, lon: -122.4194 }, // Example: San Francisco
  "7890ghij12kl": { lat: 34.0522, lon: -118.2437 }, // Example: Los Angeles
  "mnop3456qrst": { lat: 40.7128, lon: -74.0060 }   // Example: New York
  // Add more sensor chip IDs and their coordinates here
};

// Store sensor data
let sensorData = {};

// Handle POST requests from receiver node
app.post('/update', (req, res) => {
  const data = req.body;
  const sensorId = data.sensor_id;

  // Look up GPS coordinates from the table
  const location = sensorLocations[sensorId] || { lat: 0, lon: 0 }; // Default to (0,0) if unknown
  const enrichedData = {
    ...data,
    lat: location.lat,
    lon: location.lon
  };

  sensorData[sensorId] = enrichedData;
  console.log('Received and enriched data:', enrichedData);
  io.emit('sensorUpdate', enrichedData); // Broadcast to clients
  res.sendStatus(200);
});

// Serve static files (HTML, CSS, JS)
app.use(express.static('public'));

// Start server
const PORT = 3000;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on port ${PORT}`);
});
