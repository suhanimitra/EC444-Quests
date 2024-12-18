const dgram = require("dgram");
const readline = require("readline");

// ESP32 details
const ESP32_IP = "192.168.0.149"; // ESP32 IP address//nexus 
//const ESP32_IP = "192.168.0.223"; // ESP32 IP address//bitsy
const ESP32_PORT = 1111;         // Arbitrary port for ESP32 to listen on

// Create a UDP client

const udpClient = dgram.createSocket("udp4");

// Create a readline interface for terminal input
const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  prompt: "Enter 'w', 'a', 's', or 'd': "
});

console.log(`Sending commands to ESP32 at ${ESP32_IP}:${ESP32_PORT}`);
rl.prompt();

// Handle user input
rl.on("line", (line) => {
  const input = line.trim().toLowerCase();
  const validKeys = ["w", "a", "s", "d","g","q"];

  if (validKeys.includes(input)) {
    udpClient.send(input, ESP32_PORT, ESP32_IP, (err) => {
      if (err) {
        console.error("Failed to send message:", err.message);
      } else {
        console.log(`Sent '${input}' to ESP32.`);
      }
    });
  } else {
    console.log("Invalid input. Please enter 'w', 'a', 's', or 'd'.");
  }
  rl.prompt();
});

// Handle exit and cleanup
rl.on("close", () => {
  console.log("Exiting...");
  udpClient.close();
});
