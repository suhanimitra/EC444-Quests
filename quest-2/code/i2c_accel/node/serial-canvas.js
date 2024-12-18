// Serial-Canvas
// This program works with an ESP32 that sends count, time values via UART
// over the serial port to a laptop running a node server
// This code runs as 'node serial-canvas.js' and is coupled to index.html
// which includes the CanvasJS calls. The index.html also uses sockets.io
// to pass data between the node server and the client code
// v.2023-10-04

const express = require('express');
const http = require('http');
const {SerialPort} = require('serialport');
const {ReadlineParser} = require('@serialport/parser-readline');

const app = express();
const server = http.createServer(app);   // Creates web server in node
const io = require('socket.io')(server); // Uses socket.io to pass data to client

const serialPort = new SerialPort({ path: '/dev/cu.usbserial-01558F9C', baudRate: 115200 });
const port = 3000;                       // Set device ID to your particular ESP32

// Create a parser to read lines from the serial port
const parser = serialPort.pipe(new ReadlineParser({ delimiter: '\n' }));

// Serve the HTML page
app.get('/', (req, res) => {
    res.sendFile(__dirname + '/index.html');
});

// Listen for incoming WebSocket connections on the localhost port
io.on('connection', (socket) => {
    console.log('A user connected');

    // Forward data from the serial port to the socket (address:port)
    parser.on('data', (data) => {
        socket.emit('data', data);
    });

    // Handle disconnections
    socket.on('disconnect', () => {
        console.log('A user disconnected');
    });
});

// Start the server
server.listen(port, () => {
    console.log(`Server is running on http://localhost:${port}`);
    
});