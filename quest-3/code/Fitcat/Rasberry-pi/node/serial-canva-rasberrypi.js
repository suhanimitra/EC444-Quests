const express = require('express');
const http = require('http');
const socketIo = require('socket.io');

// Initialize express and the HTTP server
const app = express();
const server = http.createServer(app);
const io = socketIo(server); // Set up WebSocket for real-time communication

// Middleware to parse JSON requests
app.use(express.json());

// Store cat activity data
let cats = {};

// POST endpoint to receive data from ESP32s
app.post('/cat-data', (req, res) => {
    const { id, activityState } = req.body;
    cats[id] = activityState;
    console.log(`Received data from cat ${id}: ${activityState}`);
    res.status(200).send('Data received');
    
    // Broadcast leader info to all clients (ESP32s) if needed
    io.emit('leader-update', { leader: calculateLeader() });
});

// Function to calculate leader (simplified for example)
function calculateLeader() {
    // Dummy logic: just return the first cat
    return Object.keys(cats)[0];
}

// WebSocket event for the ESP32 to listen for updates
io.on('connection', (socket) => {
    console.log('A device connected');
    
    socket.on('disconnect', () => {
        console.log('A device disconnected');
    });
});

// Start server on port 3000
server.listen(3000, () => {
    console.log('Server listening on port 3000');
});
