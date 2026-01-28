#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

/**
 * @file web_assets.h
 * @brief Web UI assets stored in PROGMEM
 * 
 * HTML, CSS, and JavaScript for the robot control interface.
 * Stored as PROGMEM strings to save RAM.
 */

// Simple web interface HTML
const char WEB_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>SCARA Robot Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            background: #f0f0f0;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            text-align: center;
        }
        .control-panel {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin: 20px 0;
        }
        .control-group {
            padding: 15px;
            background: #f9f9f9;
            border-radius: 5px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        input[type="number"] {
            width: 100%;
            padding: 8px;
            margin-bottom: 10px;
            border: 1px solid #ddd;
            border-radius: 4px;
        }
        button {
            width: 100%;
            padding: 10px;
            background: #4CAF50;
            color: white;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 16px;
        }
        button:hover {
            background: #45a049;
        }
        button.stop {
            background: #f44336;
        }
        button.stop:hover {
            background: #da190b;
        }
        .status {
            margin-top: 20px;
            padding: 15px;
            background: #e3f2fd;
            border-radius: 5px;
        }
        .status-item {
            margin: 5px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>SCARA Robot Controller</h1>
        
        <div class="control-panel">
            <div class="control-group">
                <h3>Position Control</h3>
                <label>X (mm):</label>
                <input type="number" id="x" value="150" step="0.1">
                <label>Y (mm):</label>
                <input type="number" id="y" value="150" step="0.1">
                <label>Speed (mm/s):</label>
                <input type="number" id="speed" value="50" step="1" min="1" max="100">
                <button onclick="moveTo()">Move To Position</button>
            </div>
            
            <div class="control-group">
                <h3>Actions</h3>
                <button onclick="home()">Home Robot</button>
                <button class="stop" onclick="stop()">Emergency Stop</button>
            </div>
        </div>
        
        <div class="status" id="status">
            <h3>Status</h3>
            <div class="status-item">Position: <span id="pos">-</span></div>
            <div class="status-item">Angles: <span id="angles">-</span></div>
            <div class="status-item">Moving: <span id="moving">-</span></div>
        </div>
    </div>
    
    <script>
        let ws = null;
        
        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = protocol + '//' + window.location.hostname + '/ws';
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                console.log('WebSocket connected');
            };
            
            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                updateStatus(data);
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
            };
            
            ws.onclose = function() {
                console.log('WebSocket closed, reconnecting...');
                setTimeout(connectWebSocket, 1000);
            };
        }
        
        function moveTo() {
            const x = parseFloat(document.getElementById('x').value);
            const y = parseFloat(document.getElementById('y').value);
            const speed = parseFloat(document.getElementById('speed').value);
            
            fetch(`/move?x=${x}&y=${y}&speed=${speed}`)
                .then(response => response.text())
                .then(data => console.log('Move command sent:', data));
        }
        
        function home() {
            fetch('/home')
                .then(response => response.text())
                .then(data => console.log('Home command sent:', data));
        }
        
        function stop() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({type: 'STOP'}));
            }
        }
        
        function updateStatus(data) {
            document.getElementById('pos').textContent = 
                `(${data.x.toFixed(2)}, ${data.y.toFixed(2)})`;
            document.getElementById('angles').textContent = 
                `θ₁: ${data.theta1.toFixed(2)}°, θ₂: ${data.theta2.toFixed(2)}°`;
            document.getElementById('moving').textContent = data.isMoving ? 'Yes' : 'No';
        }
        
        // Connect on page load
        connectWebSocket();
        
        // Poll status every second
        setInterval(() => {
            fetch('/status')
                .then(response => response.json())
                .then(data => console.log('Status:', data));
        }, 1000);
    </script>
</body>
</html>
)rawliteral";

#endif // WEB_ASSETS_H
