# Testing

## Unit Tests
Unit tests for C++ code in ATOS are written in gtest. New modules must have unit tests covering at least the core functionality. All unit tests for a each module are executed automatically each time a pull request is submitted. 

To run individual unit tests, run the following command from the ATOS build folder in your workspace:
```bash
cd ~/atos_ws/build/atos
ctest -R <test_name>
```

To run all unit tests you can just run 
```bash
colcon test --event-handlers console_cohesion+
```
from the root folder of your workspace.

colcon and cmake switches if test should be built with the BUILD_TESTING flag. This should be set to ON by default. If not you can set it manually with
```bash
colcon build --cmake-args -DBUILD_TESTING=ON
```

## Sample code
In the Sample Module you will find a sample node and a some tests that triggers message callbacks and service routines. Use this as inspiration when testing your modules. 