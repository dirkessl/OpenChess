#include "sensor_test.h"
#include "chess_utils.h"
#include <Arduino.h>

SensorTest::SensorTest(BoardDriver* bd) : boardDriver(bd) {
}

void SensorTest::begin() {
  Serial.println("Starting Sensor Test Mode...");
  Serial.println("Place pieces on the board to see them light up!");
  Serial.println("This mode continuously displays detected pieces.");

  boardDriver->clearAllLEDs();
}

void SensorTest::update() {
  // Read current sensor state
  boardDriver->readSensors();

  // Update LEDs based on sensor state (no clearing to prevent flicker)
  for (int row = 0; row < 8; row++)
    for (int col = 0; col < 8; col++)
      if (boardDriver->getSensorState(row, col))
        // Light up detected pieces in white
        boardDriver->setSquareLED(row, col, 0, 0, 0, 255);
      else
        // Turn off LED where no piece is detected
        boardDriver->setSquareLED(row, col, 0, 0, 0, 0);

  // Show the updated LED state
  boardDriver->showLEDs();

  delay(50);
}