//Leyandra Burke, Suhani Mitra, Margherita Piana, Kyla Wilson
//10-25-2024

// Import the required modules
const dgram = require('dgram');
const http = require('http');
const express = require('express');
const path = require('path');
const { Server } = require('socket.io');
const { send } = require('process');

// Configuration
const UDP_PORT = 3000;             // Port to listen for UDP messages from ESP32 clients
const UDP_HOST = '192.168.1.143';  // Pi Port
const HTTP_PORT = 8080;            // HTTP server

// Initializing
const app = express();
const server = http.createServer(app);
const io = new Server(server);

// Serve HTML page
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'index.html'));
});

const udpServer = dgram.createSocket('udp4');   // Setup UDP server

// Store client information and activity logs
const clients = {};

// Activity states considered as "Active"
const ACTIVE_STATES = ['Active', 'Highly Active'];

// Rolling window configuration
const ROLLING_WINDOW_SECONDS = 600; // 10 minutes
const CLEANUP_INTERVAL_MS = 60000;  // 1 minute

// Variable to track the last leader
let lastLeader = null;

const ESP32_NAMES = {
    '192.168.1.126': 'SUHANI',
    '192.168.1.114': 'LEYANDRA',
    '192.168.1.147': 'MARGHE',
    '192.168.1.132': 'KYLA' // TO BE DETERMINED
};

// Handle UDP server listening event
udpServer.on('listening', () => {
    const address = udpServer.address();
    console.log(`UDP Server listening on ${address.address}:${address.port}`);
});

// Handle incoming UDP messages
udpServer.on('message', (message, remote) => {
    const clientKey = remote.address; // Use only the IP address
    const messageStr = message.toString().trim(); // Remove any extra whitespace

    // Initialize client tracking if new client
    if (!clients[clientKey]) {
        clients[clientKey] = {
            port: remote.port, // Store the port separately
            activityLog: [],   // Array of { timestamp, state }
            inactiveSeconds: 0,
            activeSeconds: 0,
            highlyActiveSeconds: 0
        };
        console.log(`New client connected: ${clientKey} on port ${remote.port}`);
    }

    const client = clients[clientKey];
    const currentTime = Date.now();

    // Log the current activity
    client.activityLog.push({
        timestamp: currentTime,
        state: messageStr
    });

    // Update activity state counts
    switch (messageStr) {
        case '0':
            client.inactiveSeconds += 1;
            break;
        case '1':
            client.activeSeconds += 1;
            break;
        case '2':
            client.highlyActiveSeconds += 1;
            break;
    }

    // Remove entries older than 10 minutes and adjust counts
    while (client.activityLog.length > 0 && (currentTime - client.activityLog[0].timestamp) > ROLLING_WINDOW_SECONDS * 1000) {
        const oldEntry = client.activityLog.shift();
        switch (oldEntry.state) {
            case '0':
                client.inactiveSeconds = Math.max(0, client.inactiveSeconds - 1);
                break;
            case '1':
                client.activeSeconds = Math.max(0, client.activeSeconds - 1);
                break;
            case '2':
                client.highlyActiveSeconds = Math.max(0, client.highlyActiveSeconds - 1);
                break;
        }
    }

    // Simplified Logging: Only log the received message
    console.log(`Received from ${clientKey}: ${messageStr}`);
   
    // Update leaderboard to all connected Socket.io clients
    updateLeaderboard();
});

// Handle UDP server errors
udpServer.on('error', (err) => {
    console.error(`UDP Server error:\n${err.stack}`);
    udpServer.close();
});

// Bind the UDP server
udpServer.bind(UDP_PORT, UDP_HOST);

// Function to emit leaderboard to all connected Socket.io clients
function updateLeaderboard() {
    const allClients = [];
    let leader = null;
    let maxActive = 0;

    // Construct the clients array and identify the leader
    for (const clientKey in clients) {
        const client = clients[clientKey];
        const totalActive = client.activeSeconds + client.highlyActiveSeconds;

        allClients.push({
            client: clientKey,
            inactiveSeconds: client.inactiveSeconds,
            activeSeconds: client.activeSeconds,
            highlyActiveSeconds: client.highlyActiveSeconds,
            totalActiveSeconds: totalActive
        });

        // Identify the leader based on total active seconds
        if (totalActive > maxActive) {
            maxActive = totalActive;
            leader = { client: clientKey, activeSeconds: maxActive };
        }
    }

    const response = {
        leader: leader ? leader : "No active clients",
        clients: allClients,
        timestamp: new Date().toISOString()
    };

    // Emit the leaderboardUpdate event to all connected Socket.io clients
    io.emit('leaderboardUpdate', response);

    // Determine if the leader has changed
    let isNewLeader = 0;
    if (leader) {
        if (!lastLeader || leader.client !== lastLeader.client) {
            isNewLeader = 1;
            lastLeader = leader;
        } else {
            isNewLeader = 0;
        }
    } else {
        // No leader exists
        if (lastLeader) {
            isNewLeader = 1;
            lastLeader = null;
        } else {
            isNewLeader = 0;
        }
    }

    // Emit the leaderChanged event with only the leader name
    if (isNewLeader) {
        io.emit('leaderChanged', { leader: leader ? leader.client : "No active clients" });
    }    

    // Prepare the message to send to ESP32 clients
    const leaderName = leader ? (ESP32_NAMES[leader.client] || leader.client) : "NO ACTIVE CATS";
    console.log(`Current leader IP: ${leader ? leader.client : "No active leader"}, Leader Name: ${leaderName}`);


    // Construct the message with the leader's name
    const message = `${leaderName}`;

    // Send the message to each client via UDP
    for (const clientKey in clients) {
        const address = clientKey;                     // clientKey is now just the IP
        const portNumber = clients[clientKey].port;    // Retrieve the stored port

        // Validate the port number
        if (typeof portNumber !== 'number' || portNumber <= 0 || portNumber >= 65536) {
            console.error(`Invalid port number for client ${clientKey}: ${portNumber}`);
            continue; // Skip sending to this client
        }

        udpServer.send(message, 0, message.length, portNumber, address, (err) => {
            if (err) {
                console.error(`Error sending to ${clientKey}: ${err}`);
            } else {
                const clientName = ESP32_NAMES[clientKey] || clientKey;
                console.log(`Sent to ${clientName}: ${message}`);       //sending the wrong cat name (not leader)
            }
        });
    }
}

// Periodically clean up inactive clients
setInterval(() => {
    const now = Date.now();
    let removed = false;
    for (const clientKey in clients) {
        const client = clients[clientKey];
        // Check if the last activity was more than 10 minutes ago
        if (client.activityLog.length === 0) continue;
        const lastActivity = client.activityLog[client.activityLog.length - 1].timestamp;
        if ((now - lastActivity) > ROLLING_WINDOW_SECONDS * 1000) {
            console.log(`Removing inactive client: ${clientKey}`);
            delete clients[clientKey];
            removed = true;
        }
    }
    if (removed) {
        updateLeaderboard();
    }
}, CLEANUP_INTERVAL_MS);

// Handle Socket.io connections
io.on('connection', (socket) => {
    console.log('A web client connected');

    // Send the current leaderboard upon connection
    const allClients = [];
    let leader = null;
    let maxActive = 0;

    // Construct the clients array and identify the leader
    for (const clientKey in clients) {
        const client = clients[clientKey];
        const totalActive = client.activeSeconds + client.highlyActiveSeconds;

        allClients.push({
            client: clientKey,
            inactiveSeconds: client.inactiveSeconds,
            activeSeconds: client.activeSeconds,
            highlyActiveSeconds: client.highlyActiveSeconds,
            totalActiveSeconds: totalActive
        });

        // Identify the leader based on total active seconds
        if (totalActive > maxActive) {
            maxActive = totalActive;
            leader = { client: clientKey, activeSeconds: maxActive };
        }
    }

    const response = {
        leader: leader ? leader : "No active clients",
        clients: allClients,
        timestamp: new Date().toISOString()
    };

    // Emit the leaderboardUpdate event to the connected Socket.io client
    socket.emit('leaderboardUpdate', response);
});

// Start the HTTP server
server.listen(HTTP_PORT, () => {
    console.log(`HTTP Server running at http://localhost:${HTTP_PORT}`);
});
