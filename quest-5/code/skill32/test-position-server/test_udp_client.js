const dgram = require('dgram');
const client = dgram.createSocket('udp4');

// Define the server details
const PORT = 41234;
const HOST = '192.168.0.167';

// Send a message to the server
const message = Buffer.from('ROBOTID 10');

client.send(message, PORT, HOST, (err) => {
    if (err) {
        console.error(`Error: ${err}`);
        client.close();
    } else {
        console.log(`Message sent: ${message}`);
    }
});

// Listen for a response from the server
client.on('message', (msg, rinfo) => {
    console.log(`Received response from server: ${msg}`);
    client.close(); // Close the client after receiving the response
});