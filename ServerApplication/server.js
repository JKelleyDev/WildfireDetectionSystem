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

// Predefined sensor locations (updated to be near SDSU campus)
const sensorLocations = {
  "f4e53e43ca48": { lat: 32.7757, lon: -117.0715 }, // SDSU main
  "7890ghij12kl": { lat: 32.7772, lon: -117.0678 }, // near SDSU Student Union
  "mnop3456qrst": { lat: 32.7730, lon: -117.0742 }  // near Viejas Arena
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

// Handle sensor updates (like Arduino’s POST logic)
app.post('/update', (req, res) => {
  const data = req.body;
  const sensorId = data.sensor_id;
  if (sensorData[sensorId]) {
    sensorData[sensorId].temperature = data.temperature || 0;
    sensorData[sensorId].mq9 = data.mq9 || 0;
    sensorData[sensorId].mq135 = data.mq135 || 0;
    sensorData[sensorId].fireRisk = (data.temperature > 50 || data.mq9 > 200) ? "High" : "Low";
    sensorData[sensorId].connected = true;
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
