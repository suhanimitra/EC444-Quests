const dgram = require('dgram');
const Engine = require('tingodb')();
const db = new Engine.Db('./data', {});

// Database collection
const waypointCollection = db.collection('car_data.json');

// UDP client setup
const UDP_HOST = '192.168.0.167';
const UDP_PORT = 41234;
const udpClient = dgram.createSocket('udp4');

// Function to parse OptiTrack data
function parseOptiTrackData(data) {
    const parsedData = data.toString().split(',').map(str => str.trim());
    return {
        id: parseFloat(parsedData[0]),
        x_coord: parseFloat(parsedData[1]),
        y_coord: parseFloat(parsedData[2]),
        theta: parseFloat(parsedData[3]),
        status: parseInt(parsedData[4]) === 1
    };
}

// Function to update the database with live coordinates
function updateDatabase(robotId, coordinates) {
    waypointCollection.update(
        { robot_id: robotId },
        { $set: coordinates },
        { upsert: true }, // Insert a new record if one doesn’t exist
        (err, result) => {
            if (err) {
                console.error(`Error updating data for robot ${robotId}:`, err);
            } else {
                console.log(`Live data updated for robot ${robotId}:`, coordinates);
            }
        }
    );
}

// Persistent listener for incoming UDP messages
udpClient.on('message', (data, rinfo) => {
    console.log(`Received data: ${data.toString()}`);
    const coordinates = parseOptiTrackData(data);
    coordinates.timestamp = new Date().toISOString();
    updateDatabase(coordinates.id, coordinates); // Use parsed robot ID to update the database
});

// Function to ping robots and fetch live coordinates
function fetchLiveCoordinates() {
    [11, 12].forEach(robotId => {
        const message = Buffer.from(`ROBOTID ${robotId}`);
        udpClient.send(message, UDP_PORT, UDP_HOST, err => {
            if (err) {
                console.error(`Error sending UDP message for robot ${robotId}:`, err);
            } else {
                console.log(`Successfully pinged the server for robot ${robotId}`);
            }
        });
    });
}

// Start fetching live coordinates periodically
setInterval(fetchLiveCoordinates, 5000); // Fetch live data every 5 seconds
