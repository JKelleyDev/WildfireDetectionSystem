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
app.use(express.static('public'));
app.use('/node_modules', express.static(path.join(__dirname, 'node_modules')));

// Predefined sensor locations (1.5 km apart near SDSU)
const sensorLocations = {
  "f4e53e43ca48": { lat: 32.7757, lon: -117.0715 }, // SDSU Main Campus
  "7890ghij12kl": { lat: 32.7850, lon: -117.0620 }, // North-East, ~1.5 km away
  "mnop3456qrst": { lat: 32.7650, lon: -117.0810 }  // South-West, ~1.5 km away
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
    fireRisk: "Low",
    connected: false
  };
});

// Handle sensor updates (improved fire detection logic)
app.post('/update', (req, res) => {
  const data = req.body;
  const sensorId = data.sensor_id;

  if (sensorData[sensorId]) {
    // Update the live sensor readings
    sensorData[sensorId].temperature = data.temperature || 0;
    sensorData[sensorId].mq9 = data.mq9 || 0;
    sensorData[sensorId].mq135 = data.mq135 || 0;
    sensorData[sensorId].connected = true;

    // Fire detection logic
    const temp = sensorData[sensorId].temperature;
    const mq9 = sensorData[sensorId].mq9;
    const mq135 = sensorData[sensorId].mq135;

    let fireRisk = "Low";

    // Check against thresholds
    if (temp > 55 || (mq9 > 365 * 1.5) || (mq135 > 1000 * 1.5)) {
      fireRisk = "High";
    } else if (temp > 45 || (mq9 > 365 * 1.2) || (mq135 > 1000 * 1.2)) {
      fireRisk = "Moderate";
    } else {
      fireRisk = "Low";
    }

    sensorData[sensorId].fireRisk = fireRisk;

    console.log('Sensor update:', sensorData[sensorId]);
    io.emit('sensorUpdate', sensorData[sensorId]);
  }

  res.sendStatus(200);
});


// Send initial data to clients
io.on('connection', (socket) => {
  console.log('Client connected');
  Object.values(sensorData).forEach(data => socket.emit('sensorUpdate', data));
});

const PORT = 80;
server.listen(PORT, '0.0.0.0', () => {
  console.log(`Server running on port ${PORT}`);
});
