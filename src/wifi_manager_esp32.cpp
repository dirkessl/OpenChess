#include <Arduino.h>
#include <Preferences.h>
#include "wifi_manager_esp32.h"
#include "arduino_secrets.h"

extern "C"
{
#include "nvs_flash.h"
}

WiFiManagerESP32::WiFiManagerESP32(BoardDriver *boardDriver) : server(AP_PORT)
{
    _boardDriver = boardDriver;
    apMode = true;
    clientConnected = false;
    gameMode = "None";
    boardStateValid = false;
    hasPendingEdit = false;
    boardEvaluation = 0.0;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase(); // erase and retry
        nvs_flash_init();
    }
    prefs.begin("wifiCreds", true);
    wifiSSID = prefs.getString("ssid", SECRET_SSID);
    wifiPassword = prefs.getString("pass", SECRET_PASS);
    prefs.end();

    // Initialize board state to empty
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            boardState[row][col] = ' ';
            pendingBoardEdit[row][col] = ' ';
        }
    }
}

void WiFiManagerESP32::begin()
{
    Serial.println("=== Starting OpenChess WiFi Manager (ESP32) ===");
    Serial.println("Debug: WiFi Manager begin() called");

    // ESP32 can run both AP and Station modes simultaneously
    // Start Access Point first (always available)
    Serial.print("Debug: Creating Access Point with SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Debug: Using password: ");
    Serial.println(AP_PASSWORD);

    Serial.println("Debug: Calling WiFi.softAP()...");

    // Create Access Point
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD);

    if (!apStarted)
    {
        Serial.println("ERROR: Failed to create Access Point!");
        return;
    }

    Serial.println("Debug: Access Point created successfully");

    // Try to connect to existing WiFi
    bool connected = connectToWiFi(wifiSSID, wifiPassword) ? Serial.println("Successfully connected to WiFi network!") : Serial.println("Failed to connect to WiFi. Access Point mode still available.");

    // Wait a moment for everything to stabilize
    delay(100);

    // Print connection information
    Serial.println("=== WiFi Connection Information ===");
    Serial.print("Access Point SSID: ");
    Serial.println(AP_SSID);
    Serial.print("Access Point IP: ");
    Serial.println(WiFi.softAPIP());
    if (connected)
    {
        Serial.print("Connected to WiFi: ");
        Serial.println(WiFi.SSID());
        Serial.print("Station IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("Access board via: http://");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.print("Access board via: http://");
        Serial.println(WiFi.softAPIP());
    }
    Serial.print("MAC Address: ");
    Serial.println(WiFi.softAPmacAddress());
    Serial.println("=====================================");

    // Set up web server routes with async handlers
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->handleRoot(request); });

    server.on("/game", HTTP_GET, [this](AsyncWebServerRequest *request)
              {
        String gameSelectionPage = this->generateGameSelectionPage();
        request->send(200, "text/html", gameSelectionPage); });

    server.on("/board", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->handleBoard(request); });

    server.on("/board-view", HTTP_GET, [this](AsyncWebServerRequest *request)
              { this->handleBoardView(request); });

    server.on("/board-edit", HTTP_GET, [this](AsyncWebServerRequest *request)
              {
        String boardEditPage = this->generateBoardEditPage();
        request->send(200, "text/html", boardEditPage); });

    server.on("/board-edit", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleBoardEdit(request); });

    server.on("/connect-wifi", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleConnectWiFi(request); });

    server.on("/submit", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleConfigSubmit(request); });

    server.on("/gameselect", HTTP_POST, [this](AsyncWebServerRequest *request)
              { this->handleGameSelection(request); });

    server.onNotFound([this](AsyncWebServerRequest *request)
                      {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>404 - Page Not Found</h2>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
        response += "</body></html>";
        request->send(404, "text/html", response); });

    // Start the web server
    Serial.println("Debug: Starting web server...");
    server.begin();
    Serial.println("Debug: Web server started on port 80");
    Serial.println("WiFi Manager initialization complete!");
}

void WiFiManagerESP32::handleRoot(AsyncWebServerRequest *request)
{
    String webpage = generateWebPage();
    sendResponse(request, webpage);
}

void WiFiManagerESP32::handleConfigSubmit(AsyncWebServerRequest *request)
{
    // Parse form data from async request
    if (request->hasArg("ssid"))
        wifiSSID = request->arg("ssid");
    if (request->hasArg("password"))
        wifiPassword = request->arg("password");
    Serial.println("Configuration updated:");
    Serial.println("SSID: " + wifiSSID);
    Serial.println("Password: " + wifiPassword);
    prefs.begin("wifiCreds", false);
    prefs.putString("ssid", wifiSSID);
    prefs.putString("pass", wifiPassword);
    prefs.end();

    String html = R"rawliteral(
<html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>OpenChess</title>
        <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #5c5d5e;
                margin: 0;
                padding: 0;
                display: flex;
                justify-content: center;
                align-items: center;
                min-height: 100vh;
            }

            .container {
                background-color: #353434;
                border-radius: 8px;
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
                padding: 30px;
                width: 100%;
                max-width: 500px;
            }

            h2 {
                text-align: center;
                color: #ec8703;
                font-size: 30px;
                margin-bottom: 20px;
            }

            h3 {
                text-align: center;
                color: #ec8703;
                font-size: 16px;
                margin-bottom: 20px;
            }

            label {
                font-size: 16px;
                color: #ec8703;
                margin-bottom: 8px;
                display: block;
            }
            
            input[type="submit"], .button {
                background-color: #ec8703;
                color: white;
                border: none;
                padding: 15px;
                font-size: 16px;
                width: 100%;
                border-radius: 5px;
                cursor: pointer;
                transition: background-color 0.3s ease;
                text-decoration: none;
                display: block;
                text-align: center;
                margin: 10px auto;
                box-sizing: border-box;
            }

            input[type="submit"]:hover, .button:hover {
                background-color: #ebca13;
            }
        </style>
    </head>
    <body>
        <div class="container">
        <h2>Configuration Saved!</h2>
        <h3>
            <span>WiFi SSID:</span>
            <span style="color:#ffffff;">%SSID%</span>
        </h3>
            <h3>
            <span>WiFi Password:</span>
            <span style="color:#ffffff;">%PASSWORD%</span>
        </h3>
        <a href='/game' class="button">GameMode Selection</a>
        </div>  
    </body>
</html>
)rawliteral";
    html.replace("%SSID%", wifiSSID);
    html.replace("%PASSWORD%", wifiPassword);
    sendResponse(request, html);
}

void WiFiManagerESP32::handleGameSelection(AsyncWebServerRequest *request)
{
    int mode = 0;

    if (request->hasArg("gamemode"))
    {
        mode = request->arg("gamemode").toInt();
    }

    Serial.print("Game mode selected via web: ");
    Serial.println(mode);

    gameMode = String(mode);

    String response = "{\"status\":\"success\",\"message\":\"Game mode selected\",\"mode\":" + String(mode) + "}";
    sendResponse(request, response, "application/json");
}

void WiFiManagerESP32::sendResponse(AsyncWebServerRequest *request, String content, String contentType)
{
    request->send(200, contentType, content);
}

String WiFiManagerESP32::generateWebPage()
{
    String html = R"rawliteral(
    <html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>OpenChess</title>
        <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #5c5d5e;
                margin: 0;
                padding: 0;
                display: flex;
                justify-content: center;
                align-items: center;
                min-height: 100vh;
            }

            .container {
                background-color: #353434;
                border-radius: 8px;
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
                padding: 30px;
                width: 100%;
                max-width: 500px;
            }

            h2 {
                text-align: center;
                color: #ec8703;
                font-size: 30px;
                margin-bottom: 20px;
            }

            label {
                font-size: 16px;
                color: #ec8703;
                margin-bottom: 8px;
                display: block;
            }

            input[type="text"], input[type="password"], select {
                width: 100%;
                padding: 10px;
                margin: 10px 0;
                border: 1px solid #ccc;
                border-radius: 5px;
                box-sizing: border-box;
                font-size: 16px;
            }

            input[type="submit"], .button {
                background-color: #ec8703;
                color: white;
                border: none;
                padding: 15px;
                font-size: 16px;
                width: 100%;
                border-radius: 5px;
                cursor: pointer;
                transition: background-color 0.3s ease;
                text-decoration: none;
                display: block;
                text-align: center;
                margin: 10px auto;
                box-sizing: border-box;
            }

            input[type="submit"]:hover, .button:hover {
                background-color: #ebca13;
            }

            .form-group {
                margin-bottom: 15px;
            }

            .note {
                font-size: 14px;
                color: #ec8703;
                text-align: center;
                margin-top: 20px;
            }
        </style>
    </head>
    <body>
        <div class="container">
            <h2>OpenChess</h2>
            <form action="/connect-wifi" method="POST">
                <div class="form-group">
                    <label for="ssid">WiFi SSID:</label>
                    <input type="text" name="ssid" id="ssid" value="%SSID%" placeholder="Enter Your WiFi SSID">
                </div>
                <div class="form-group">
                    <label for="password">WiFi Password:</label>
                    <input type="text" name="password" id="password" value="%PASSWORD%" placeholder="Enter Your WiFi Password">
                </div>
                <input type="submit" value="Connect to WiFi">
            </form>
            <div class="form-group" style="margin-top: 30px; padding: 15px; background-color: #444; border-radius: 5px;">
                <h3 style="color: #ec8703; margin-top: 0;">WiFi Connection</h3>
                <p style="color: #ec8703;">Status: %STATUS%</p>
            </div>
            <a href="/game" class="button">GameMode Selection</a>
            <a href="/board-view" class="button">View Chess Board</a>
        </div>
    </body>
</html>
    )rawliteral";
    html.replace("%SSID%", wifiSSID);
    html.replace("%PASSWORD%", wifiPassword);
    html.replace("%STATUS%", getConnectionStatus());
    return html;
}

String WiFiManagerESP32::generateGameSelectionPage()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenChess GameMode Selection</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #5c5d5e;
            margin: 0;
            padding: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }

        .container {
            background-color: #353434;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            padding: 30px;
            width: 100%;
            max-width: 600px;
        }

        h2 {
            text-align: center;
            color: #ec8703;
            font-size: 30px;
            margin-bottom: 30px;
        }

        .game-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-bottom: 30px;
        }

        .game-mode {
            background-color: #444;
            border: 2px solid #ec8703;
            border-radius: 8px;
            padding: 20px;
            text-align: center;
            cursor: pointer;
            transition: all 0.3s ease;
            color: #fff;
        }

        .game-mode:hover {
            background-color: #ec8703;
            transform: translateY(-2px);
        }

        .game-mode.available {
            border-color: #4CAF50;
        }

        .game-mode.coming-soon {
            border-color: #888;
            opacity: 0.6;
        }

        .game-mode.mode-1 {
            border-color: #FF9800;
            background: linear-gradient(135deg, #444 0%, #FF9800 100%);
        }

        .game-mode.mode-2 {
            border-color: #FFFFFF;
            background: linear-gradient(135deg, #444 0%, #FFFFFF 100%);
        }

        .game-mode.mode-3 {
            border-color: #2196F3;
            background: linear-gradient(135deg, #444 0%, #2196F3 100%);
        }

        .game-mode.mode-4 {
            border-color: #F44336;
            background: linear-gradient(135deg, #444 0%, #F44336 100%);
        }

        .game-mode h3 {
            margin: 0 0 10px 0;
            font-size: 18px;
        }

        .game-mode p {
            margin: 0;
            font-size: 14px;
            opacity: 0.8;
        }

        .status {
            font-size: 12px;
            padding: 5px 10px;
            border-radius: 15px;
            margin-top: 10px;
            display: inline-block;
        }

        .available .status {
            background-color: #4CAF50;
            color: white;
        }

        .coming-soon .status {
            background-color: #888;
            color: white;
        }

        .button {
            background-color: #ec8703;
            color: white;
            border: none;
            padding: 15px;
            font-size: 16px;
            width: 100%;
            border-radius: 5px;
            cursor: pointer;
            transition: background-color 0.3s ease;
            text-decoration: none;
            display: block;
            text-align: center;
            margin: 10px auto;
            box-sizing: border-box;
        }

        input[type="submit"]:hover,
        .button:hover {
            background-color: #ebca13;
        }

        .back-button {
            background-color: #666;
            color: white;
            border: none;
            padding: 15px;
            font-size: 16px;
            width: 100%;
            border-radius: 5px;
            cursor: pointer;
            text-decoration: none;
            display: block;
            text-align: center;
            margin-top: 20px;
            box-sizing: border-box;
        }

        .back-button:hover {
            background-color: #777;
        }
    </style>
</head>

<body>
    <div class="container">
        <h2>GameMode Selection</h2>
        <div class="game-grid">
            <div class="game-mode available mode-1" onclick="selectGame(1)">
                <h3>Chess Moves</h3>
                <p>Human vs Human</p>
                <p>Visualize available moves</p>
            </div>
            <div class="game-mode available mode-2" onclick="selectGame(2)">
                <h3>White Bot</h3>
                <p>Human vs White Bot</p>
                <p>(Stockfish Medium)</p>
            </div>
            <div class="game-mode available mode-3" onclick="selectGame(3)">
                <h3>Black Bot</h3>
                <p>Human vs Black Bot</p>
                <p>(Stockfish Medium)</p>
            </div>
            <div class="game-mode available mode-4" onclick="selectGame(4)">
                <h3>Sensor Test</h3>
                <p>Test board sensors</p>
            </div>
        </div>
        <a href="/board-view" class="button">View Chess Board</a>
        <a href="/" class="back-button">Back to Configuration</a>
    </div>
    <script>
        function selectGame(mode) {
            if (mode === 1 || mode === 2 || mode === 3 || mode === 4) {
                fetch('/gameselect', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded'
                    },
                    body: 'gamemode=' + mode
                }).then(response => response.text()).then(data => {
                    alert('Game mode ' + mode + ' selected! Check your chess board.');
                }
                ).catch(error => {
                    console.error('Error:', error);
                }
                );
            } else {
                alert('This game mode is coming soon!');
            }
        }
    </script>
</body>
</html>
    )rawliteral";

    return html;
}

bool WiFiManagerESP32::isClientConnected()
{
    return WiFi.softAPgetStationNum() > 0;
}

int WiFiManagerESP32::getSelectedGameMode()
{
    return gameMode.toInt();
}

void WiFiManagerESP32::resetGameSelection()
{
    gameMode = "0";
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8])
{
    updateBoardState(newBoardState, 0.0);
}

void WiFiManagerESP32::updateBoardState(char newBoardState[8][8], float evaluation)
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            boardState[row][col] = newBoardState[row][col];
        }
    }
    boardStateValid = true;
    boardEvaluation = evaluation;
}

void WiFiManagerESP32::handleBoard(AsyncWebServerRequest *request)
{
    String json = "{";
    json += "\"board\":[";

    for (int row = 0; row < 8; row++)
    {
        json += "[";
        for (int col = 0; col < 8; col++)
        {
            char piece = boardState[row][col];
            if (piece == ' ')
            {
                json += "\"\"";
            }
            else
            {
                json += "\"";
                json += String(piece);
                json += "\"";
            }
            if (col < 7)
                json += ",";
        }
        json += "]";
        if (row < 7)
            json += ",";
    }

    json += "],";
    json += "\"valid\":" + String(boardStateValid ? "true" : "false");
    json += ",\"evaluation\":" + String(boardEvaluation, 2);
    json += "}";
    sendResponse(request, json, "application/json");
}

void WiFiManagerESP32::handleBoardView(AsyncWebServerRequest *request)
{
    String boardViewPage = generateBoardViewPage();
    sendResponse(request, boardViewPage, "text/html");
}

String WiFiManagerESP32::generateBoardViewPage()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenChess Board View</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            background-color: #5c5d5e;
            margin: 0;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }

        .container {
            background-color: #353434;
            border-radius: 8px;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
            padding: 30px;
        }

        h2 {
            text-align: center;
            color: #ec8703;
            font-size: 24px;
            margin-bottom: 20px;
        }

        .board-container {
            display: inline-block;
        }

        .board {
            display: grid;
            grid-template-columns: repeat(8, 1fr);
            gap: 0;
            border: 3px solid #ec8703;
            width: 480px;
            height: 480px;
        }

        .square {
            width: 60px;
            height: 60px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 40px;
            font-weight: bold;
        }

        .square.light {
            background-color: #f0d9b5;
        }

        .square.dark {
            background-color: #b58863;
        }

        .square .piece {
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5);
        }

        .square .piece.white {
            color: #ffffff;
        }

        .square .piece.black {
            color: #000000;
        }

        .info {
            text-align: center;
            color: #ec8703;
            margin-top: 20px;
            font-size: 14px;
        }

        .button {
            background-color: #ec8703;
            color: white;
            border: none;
            padding: 15px;
            font-size: 16px;
            width: 100%;
            border-radius: 5px;
            cursor: pointer;
            transition: background-color 0.3s ease;
            text-decoration: none;
            display: block;
            text-align: center;
            margin: 10px auto;
            margin-top: 20px;
            box-sizing: border-box;
        }

        .back-button {
            background-color: #666;
            color: white;
            border: none;
            padding: 15px;
            font-size: 16px;
            width: 100%;
            border-radius: 5px;
            cursor: pointer;
            text-decoration: none;
            display: block;
            text-align: center;
            margin-top: 20px;
            box-sizing: border-box;
        }

        .back-button:hover {
            background-color: #777;
        }

        .status {
            text-align: center;
            color: #ec8703;
            margin-bottom: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h2>Chess Board</h2>
        %BOARD_HTML%
        <div class="info">
            <div id="evaluation" style="margin-top: 15px; padding: 15px; background-color: #444; border-radius: 5px;">
                <div style="text-align: center; margin-bottom: 10px; color: #ec8703; font-weight: bold;">Stockfish
                    Evaluation</div>
                <div
                    style="position: relative; width: 100%; height: 40px; background-color: #333; border: 2px solid #555; border-radius: 5px; overflow: hidden;">
                    <div id="eval-bar"
                        style="position: absolute; top: 0; left: 50%; width: 0%; height: 100%; background: linear-gradient(to right, #F44336 0%, #F44336 50%, #4CAF50 50%, #4CAF50 100%); transition: width 0.3s ease, left 0.3s ease;">
                    </div>
                    <div
                        style="position: absolute; top: 0; left: 50%; width: 2px; height: 100%; background-color: #ec8703; z-index: 2;">
                    </div>
                    <div id="eval-arrow"
                        style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-size: 24px; z-index: 3; color: #ec8703; transition: left 0.3s ease;">
                        ⬌</div>
                </div>
                <div id="eval-text" style="text-align: center; margin-top: 10px; font-size: 14px; color: #ec8703;">--
                </div>
            </div>
        </div>
        <a href="/board-edit" class="button">Edit Board</a>
        <a href="/" class="back-button">Back to Configuration</a>
    </div>

    <script>
        // Fetch board state via AJAX for smoother updates
        function updateBoard() {
            fetch('/board').then(response => response.json()).then(data => {
                if (data.valid) {
                    // Update board display
                    const squares = document.querySelectorAll('.square');
                    let index = 0;
                    for (let row = 0; row < 8; row++) {
                        for (let col = 0; col < 8; col++) {
                            const piece = data.board[row][col];
                            const square = squares[index];
                            if (piece && piece !== '') {
                                const isWhite = piece === piece.toUpperCase();
                                square.innerHTML = '<span class="piece ' + (isWhite ? 'white' : 'black') + '">' + getPieceSymbol(piece) + '</span>';
                            } else {
                                square.innerHTML = '';
                            } index++;
                        }
                    }
                    // Update evaluation bar
                    if (data.evaluation !== undefined) {
                        const evalValue = data.evaluation;
                        const evalInPawns = (evalValue / 100).toFixed(2);
                        const maxEval = 1000;
                        // Maximum evaluation to display (10 pawns)
                        const clampedEval = Math.max(-maxEval, Math.min(maxEval, evalValue));
                        const percentage = Math.abs(clampedEval) / maxEval * 50;
                        // Max 50% on each side
                        const bar = document.getElementById('eval-bar');
                        const arrow = document.getElementById('eval-arrow');
                        const text = document.getElementById('eval-text');
                        let evalText = '';
                        let arrowSymbol = '⬌';
                        if (evalValue > 0) {
                            bar.style.left = '50%';
                            bar.style.width = percentage + '%';
                            bar.style.background = 'linear-gradient(to right, #ec8703 0%, #4CAF50 100%)';
                            arrow.style.left = (50 + percentage) + '%';
                            arrowSymbol = '→';
                            arrow.style.color = '#4CAF50';
                            evalText = '+' + evalInPawns + ' (White advantage)';
                        } else if (evalValue < 0) {
                            bar.style.left = (50 - percentage) + '%';
                            bar.style.width = percentage + '%';
                            bar.style.background = 'linear-gradient(to right, #F44336 0%, #ec8703 100%)';
                            arrow.style.left = (50 - percentage) + '%';
                            arrowSymbol = '←';
                            arrow.style.color = '#F44336';
                            evalText = evalInPawns + ' (Black advantage)';
                        } else {
                            bar.style.left = '50%';
                            bar.style.width = '0%';
                            bar.style.background = '#ec8703';
                            arrow.style.left = '50%';
                            arrowSymbol = '⬌';
                            arrow.style.color = '#ec8703';
                            evalText = '0.00 (Equal)';
                        } arrow.textContent = arrowSymbol;
                        text.textContent = evalText;
                        text.style.color = arrow.style.color;
                    }
                }
            });
        }
        
        function getPieceSymbol(piece) {
            if (!piece) return '';
            const symbols = { 'R': '♖', 'N': '♘', 'B': '♗', 'Q': '♕', 'K': '♔', 'P': '♙', 'r': '♖', 'n': '♘', 'b': '♗', 'q': '♕', 'k': '♔', 'p': '♙' };
            return symbols[piece] || piece;
        }
        
        setInterval(updateBoard, 250);
    </script>
</body>
</html>
    )rawliteral";

    String boardHTML = "";
    if (boardStateValid)
    {
        boardHTML += "<div class=\"status\">Board state: Active</div>";
        // Show evaluation if available (for Chess Bot mode)
        if (boardEvaluation != 0.0)
        {
            float evalInPawns = boardEvaluation / 100.0;
            String evalColor = "#ec8703";
            String evalText = "";
            if (boardEvaluation > 0)
            {
                evalText = "+" + String(evalInPawns, 2) + " (White advantage)";
                evalColor = "#4CAF50";
            }
            else
            {
                evalText = String(evalInPawns, 2) + " (Black advantage)";
                evalColor = "#F44336";
            }
            boardHTML += "<div class=\"status\" style=\"color: " + evalColor + ";\">";
            boardHTML += "Stockfish Evaluation: " + evalText;
            boardHTML += "</div>";
        }
        boardHTML += "<div class=\"board-container\">";
        boardHTML += "<div class=\"board\">";

        // Generate board squares
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                bool isLight = (row + col) % 2 == 0;
                char piece = boardState[row][col];

                boardHTML += "<div class=\"square " + String(isLight ? "light" : "dark") + "\">";

                if (piece != ' ')
                {
                    bool isWhite = (piece >= 'A' && piece <= 'Z');
                    String pieceSymbol = getPieceSymbol(piece);
                    boardHTML += "<span class=\"piece " + String(isWhite ? "white" : "black") + "\">" + pieceSymbol + "</span>";
                }

                boardHTML += "</div>";
            }
        }

        boardHTML += "</div>";
        boardHTML += "</div>";
    }
    else
    {
        boardHTML += "<div class=\"status\">Board state: Not available</div>";
        boardHTML += "<p style=\"text-align: center; color: #ec8703;\">No active game detected. Start a game to view the board.</p>";
    }
    html.replace("%BOARD_HTML%", boardHTML);

    return html;
}

String WiFiManagerESP32::getPieceSymbol(char piece)
{
    switch (piece)
    {
    case 'R':
        return "♖"; // White Rook
    case 'N':
        return "♘"; // White Knight
    case 'B':
        return "♗"; // White Bishop
    case 'Q':
        return "♕"; // White Queen
    case 'K':
        return "♔"; // White King
    case 'P':
        return "♙"; // White Pawn
    case 'r':
        return "♖"; // Black Rook
    case 'n':
        return "♘"; // Black Knight
    case 'b':
        return "♗"; // Black Bishop
    case 'q':
        return "♕"; // Black Queen
    case 'k':
        return "♔"; // Black King
    case 'p':
        return "♙"; // Black Pawn
    default:
        return String(piece);
    }
}

String WiFiManagerESP32::generateBoardEditPage()
{
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Edit Chess Board</title>
        <style>
            body {
                font-family: Arial, sans-serif;
                background-color: #5c5d5e;
                margin: 0;
                padding: 20px;
            }

            .container {
                background-color: #353434;
                border-radius: 8px;
                box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
                padding: 30px;
                max-width: 800px;
                margin: 0 auto;
            }

            h2 {
                text-align: center;
                color: #ec8703;
                font-size: 24px;
                margin-bottom: 20px;
            }

            .board-container {
                display: inline-block;
                margin: 20px auto;
            }

            .board {
                display: grid;
                grid-template-columns: repeat(8, 1fr);
                gap: 0;
                border: 3px solid #ec8703;
                width: 480px;
                height: 480px;
            }

            .square {
                width: 60px;
                height: 60px;
                display: flex;
                align-items: center;
                justify-content: center;
                position: relative;
            }

            .square.light {
                background-color: #f0d9b5;
            }

            .square.dark {
                background-color: #b58863;
            }

            .square:hover {
                background-color: #ec8703 !important;
                opacity: 0.8;
            }

            .square select {
                width: 100%;
                height: 100%;
                border: none;
                background: transparent;
                font-size: 32px;
                text-align: center;
                cursor: pointer;
                appearance: none;
                -webkit-appearance: none;
                -moz-appearance: none;
            }

            .square select:focus {
                outline: 2px solid #ec8703;
            }

            .controls {
                text-align: center;
                margin-top: 20px;
            }

            .button {
                background-color: #ec8703;
                color: white;
                border: none;
                padding: 15px 30px;
                font-size: 16px;
                border-radius: 5px;
                cursor: pointer;
                margin: 10px;
            }

            .button:hover {
                background-color: #ebca13;
            }

            .button.secondary {
                background-color: #666;
            }

            .button.secondary:hover {
                background-color: #777;
            }

            .info {
                text-align: center;
                color: #ec8703;
                margin-top: 20px;
                font-size: 14px;
            }

            .back-button {
                background-color: #666;
                color: white;
                border: none;
                padding: 15px;
                font-size: 16px;
                width: 100%;
                border-radius: 5px;
                cursor: pointer;
                text-decoration: none;
                display: block;
                text-align: center;
                margin-top: 20px;
                box-sizing: border-box;
            }

            .back-button:hover {
                background-color: #777;
            }

            .status {
                text-align: center;
                color: #ec8703;
                margin-bottom: 20px;
                padding: 10px;
                background-color: #444;
                border-radius: 5px;
            }
        </style>
    </head>
    <body>
        <div class="container">
            <h2>Edit Chess Board</h2>
            <div class="status">Click on any square to change the piece. Empty = no piece.</div>
            <form id="boardForm" method="POST" action="/board-edit">
                <div class="board-container">
                    <div class="board">
                    %BOARD_HTML%
                    </div>
                </div>
                <div class="controls">
                    <button type="submit" class="button">Apply Changes</button>
                    <button type="button" class="button secondary" onclick="loadCurrentBoard()">Reload Current Board</button>
                    <button type="button" class="button secondary" onclick="clearBoard()">Clear All</button>
                </div>
            </form>
            <div class="info">
                <p>
                    <strong>Instructions:</strong>
                </p>
                <p>• Uppercase letters (R,N,B,Q,K,P) = White pieces</p>
                <p>• Lowercase letters (r,n,b,q,k,p) = Black pieces</p>
                <p>• Empty = No piece on that square</p>
                <p>• Click 'Apply Changes' to update the physical board</p>
            </div>
            <a href="/board-view" class="back-button">View Board</a>
            <a href="/" class="back-button">Back to Configuration</a>
        </div>
        <script>
            function loadCurrentBoard() {
                fetch('/board').then(response => response.json()).then(data => {
                    if (data.valid) {
                        for (let row = 0; row < 8; row++) {
                            for (let col = 0; col < 8; col++) {
                                const piece = data.board[row][col] || '';
                                const select = document.getElementById('r' + row + 'c' + col);
                                select.value = piece;
                            }
                        }
                    }
                }
                );
            }
            function clearBoard() {
                if (confirm('Clear all pieces from the board?')) {
                    for (let row = 0; row < 8; row++) {
                        for (let col = 0; col < 8; col++) {
                            document.getElementById('r' + row + 'c' + col).value = '';
                        }
                    }
                }
            }
        </script>
    </body>
</html>
    )rawliteral";

    // Generate editable board squares
    String boardHTML = "";
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            bool isLight = (row + col) % 2 == 0;
            char piece = boardState[row][col];
            boardHTML += "<div class=\"square " + String(isLight ? "light" : "dark") + "\">";
            boardHTML += "<select name=\"r" + String(row) + "c" + String(col) + "\" id=\"r" + String(row) + "c" + String(col) + "\">";
            boardHTML += "<option value=\"\"" + String(piece == ' ' ? " selected" : "") + "></option>";
            boardHTML += "<option value=\"R\"" + String(piece == 'R' ? " selected" : "") + ">♖ R</option>";
            boardHTML += "<option value=\"N\"" + String(piece == 'N' ? " selected" : "") + ">♘ N</option>";
            boardHTML += "<option value=\"B\"" + String(piece == 'B' ? " selected" : "") + ">♗ B</option>";
            boardHTML += "<option value=\"Q\"" + String(piece == 'Q' ? " selected" : "") + ">♕ Q</option>";
            boardHTML += "<option value=\"K\"" + String(piece == 'K' ? " selected" : "") + ">♔ K</option>";
            boardHTML += "<option value=\"P\"" + String(piece == 'P' ? " selected" : "") + ">♙ P</option>";
            boardHTML += "<option value=\"r\"" + String(piece == 'r' ? " selected" : "") + ">♜ r</option>";
            boardHTML += "<option value=\"n\"" + String(piece == 'n' ? " selected" : "") + ">♞ n</option>";
            boardHTML += "<option value=\"b\"" + String(piece == 'b' ? " selected" : "") + ">♝ b</option>";
            boardHTML += "<option value=\"q\"" + String(piece == 'q' ? " selected" : "") + ">♛ q</option>";
            boardHTML += "<option value=\"k\"" + String(piece == 'k' ? " selected" : "") + ">♚ k</option>";
            boardHTML += "<option value=\"p\"" + String(piece == 'p' ? " selected" : "") + ">♟ p</option>";
            boardHTML += "</select>";
            boardHTML += "</div>";
        }
    }
    html.replace("%BOARD_HTML%", boardHTML);

    return html;
}

void WiFiManagerESP32::handleBoardEdit(AsyncWebServerRequest *request)
{
    parseBoardEditData(request);

    String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
    response += "<h2>Board Updated!</h2>";
    response += "<p>Your board changes have been applied.</p>";
    response += "<p><a href='/board-view' style='color:#ec8703;'>View Board</a></p>";
    response += "<p><a href='/board-edit' style='color:#ec8703;'>Edit Again</a></p>";
    response += "<p><a href='/' style='color:#ec8703;'>Back to Home</a></p>";
    response += "</body></html>";
    sendResponse(request, response);
}

void WiFiManagerESP32::parseBoardEditData(AsyncWebServerRequest *request)
{
    for (int row = 0; row < 8; row++)
    {
        for (int col = 0; col < 8; col++)
        {
            String paramName = "r" + String(row) + "c" + String(col);

            if (request->hasArg(paramName.c_str()))
            {
                String value = request->arg(paramName.c_str());
                if (value.length() > 0)
                {
                    pendingBoardEdit[row][col] = value.charAt(0);
                }
                else
                {
                    pendingBoardEdit[row][col] = ' ';
                }
            }
            else
            {
                pendingBoardEdit[row][col] = ' ';
            }
        }
    }

    hasPendingEdit = true;
    Serial.println("Board edit received and stored");
}

bool WiFiManagerESP32::getPendingBoardEdit(char editBoard[8][8])
{
    if (hasPendingEdit)
    {
        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                editBoard[row][col] = pendingBoardEdit[row][col];
            }
        }
        return true;
    }
    return false;
}

void WiFiManagerESP32::clearPendingEdit()
{
    hasPendingEdit = false;
}

bool WiFiManagerESP32::connectToWiFi(String ssid, String password)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Already connected to WiFi");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        apMode = false; // We're connected, but AP is still running
        return true;
    }
    Serial.println("=== Connecting to WiFi Network ===");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);

    // ESP32 can run both AP and Station modes simultaneously
    WiFi.mode(WIFI_AP_STA); // Enable both AP and Station modes

    WiFi.begin(ssid.c_str(), password.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
        _boardDriver->showConnectingAnimation();
        attempts++;
        Serial.print("Connection attempt ");
        Serial.print(attempts);
        Serial.print("/10 - Status: ");
        Serial.println(WiFi.status());
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("Connected to WiFi!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        apMode = false; // We're connected, but AP is still running
        return true;
    }
    else
    {
        Serial.println("Failed to connect to WiFi");
        // AP mode is still available
        return false;
    }
}

bool WiFiManagerESP32::startAccessPoint()
{
    // AP is already started in begin(), this is just for compatibility
    return WiFi.softAP(AP_SSID, AP_PASSWORD);
}

IPAddress WiFiManagerESP32::getIPAddress()
{
    // Return station IP if connected, otherwise AP IP
    if (WiFi.status() == WL_CONNECTED)
    {
        return WiFi.localIP();
    }
    else
    {
        return WiFi.softAPIP();
    }
}

bool WiFiManagerESP32::isConnectedToWiFi()
{
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManagerESP32::getConnectionStatus()
{
    String status = "";
    if (WiFi.status() == WL_CONNECTED)
    {
        status = "Connected to: " + WiFi.SSID() + " (IP: " + WiFi.localIP().toString() + ")";
        status += " | AP also available at: " + WiFi.softAPIP().toString();
    }
    else
    {
        status = "Access Point Mode - SSID: " + String(AP_SSID) + " (IP: " + WiFi.softAPIP().toString() + ")";
    }
    return status;
}

void WiFiManagerESP32::handleConnectWiFi(AsyncWebServerRequest *request)
{
    if (request->hasArg("ssid"))
    {
        wifiSSID = request->arg("ssid");
    }
    if (request->hasArg("password"))
    {
        wifiPassword = request->arg("password");
    }

    if (wifiSSID.length() > 0 && wifiPassword.length() > 0)
    {
        Serial.println("Attempting to connect to WiFi from web interface...");
        bool connected = connectToWiFi(wifiSSID, wifiPassword);

        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        if (connected)
        {
            response += "<h2>WiFi Connected!</h2>";
            response += "<p>Successfully connected to: " + wifiSSID + "</p>";
            response += "<p>Station IP Address: " + WiFi.localIP().toString() + "</p>";
            response += "<p>Access Point still available at: " + WiFi.softAPIP().toString() + "</p>";
            response += "<p>You can access the board at either IP address.</p>";
            prefs.begin("wifiCreds", false);
            prefs.putString("ssid", wifiSSID);
            prefs.putString("pass", wifiPassword);
            prefs.end();
        }
        else
        {
            response += "<h2>WiFi Connection Failed</h2>";
            response += "<p>Could not connect to: " + wifiSSID + "</p>";
            response += "<p>Please check your credentials and try again.</p>";
            response += "<p>Access Point mode is still available at: " + WiFi.softAPIP().toString() + "</p>";
        }
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(request, response);
    }
    else
    {
        String response = "<html><body style='font-family:Arial;background:#5c5d5e;color:#ec8703;text-align:center;padding:50px;'>";
        response += "<h2>Error</h2>";
        response += "<p>No WiFi SSID or Password provided.</p>";
        response += "<p><a href='/' style='color:#ec8703;'>Back to Configuration</a></p>";
        response += "</body></html>";
        sendResponse(request, response);
    }
}
