#ifndef SENSOR_TEST_H
#define SENSOR_TEST_H

#include "board_driver.h"

// ---------------------------
// Sensor Test Mode Class
// ---------------------------
class SensorTest {
 private:
  BoardDriver* boardDriver;

 public:
  SensorTest(BoardDriver* bd);
  void begin();
  void update();
};

#endif // SENSOR_TEST_H