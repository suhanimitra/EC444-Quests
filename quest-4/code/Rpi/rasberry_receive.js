const express = require('express');
const fs = require('fs');
const dgram = require('dgram');
const path = require('path');
const TingoDB = require('tingodb')().Db;
const http = require('http');      // Required for setting up Socket.IO with HTTP
const socketIo = require('socket.io');

const app = express();
const server = http.createServer(app); // Use HTTP server with Express
const io = socketIo(server);           // Initialize Socket.IO
const UDP_PORT = 3000;
const UDP_HOST = '192.168.1.143';
const HTTP_PORT = 8080;


// ESP32 client names based on their IP addresses
const ESP32_NAMES = {
    '192.168.1.126': 'SUHANI',
    '192.168.1.114': 'LEYANDRA',
    '192.168.1.147': 'MARGHE',
    '192.168.1.132': 'KYLA'
};

// Initialize TingoDB
const db = new TingoDB('./db', {});
const sensorMessages = db.collection('sensorMessages');
const votes = db.collection('votes');

// UDP server setup
const udpServer = dgram.createSocket('udp4');

// Serve static HTML files
app.use(express.static(path.join(__dirname, 'public')));  // 'public' folder for static assets (e.g., your HTML, CSS, JS)

// Socket.IO handling for real-time updates
udpServer.on('message', (message, remote) => {
    const clientKey = remote.address;
    const messageStr = message.toString().trim();
    const clientName = ESP32_NAMES[clientKey] || "Unknown";
    const timestamp = new Date().toISOString();

    console.log(`Received from ${clientName} (${clientKey}): ${messageStr}`);

    const messageData = {
        clientName,
        clientKey,
        message: messageStr,
        timestamp
    };

    // Insert message data into the database
    sensorMessages.insert(messageData, (err) => {
        if (!err) {
            io.emit('newMessage', messageData); // Emit message to all connected clients via Socket.IO
        }
    });
});

// Bind UDP server
udpServer.bind(UDP_PORT, UDP_HOST);

// Routes
app.delete('/reset', (req, res) => {
    // Clear all votes in the database
    votes.remove({}, { multi: true }, (err) => {
        if (err) {
            return res.status(500).json({ error: 'Failed to reset database' });
        }
        res.status(200).json({ message: 'Database reset successfully' });
    });
});

// Serve the HTML page
app.get('/', (req, res) => {
    res.sendFile(__dirname + '/prova.html');
});

// Start the HTTP and Socket.IO server
server.listen(HTTP_PORT, () => {
    console.log(`Server running at http://192.168.1.143:${HTTP_PORT}`);
});
