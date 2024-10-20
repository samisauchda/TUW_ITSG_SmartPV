# TUW ITSG SmartPV

This project was developed for **TU Wien** as part of the Informationstechnik in Smart Grids course. It is designed to track the performance of solar panels paired with an **EMH ED300L energy meter**. The system measures the maximum power of the solar panels and compares the results with **PV GIS** data for performance analysis and optimization.

## Features

- **ESP32**-based system for solar panel performance monitoring.
- Real-time comparison with data from **PV GIS** to ensure optimal energy production.
- Code developed using **PlatformIO**.
- Supports interfacing with the **EMH ED300L** energy meter for accurate power readings.
- Additional hardware components are required for installation but are not part of the repository.

## Project Structure

- **src/**: Contains the main source code files.
- **lib/**: Holds external libraries used in the project.
- **data/**: Reserved for data storage related to measurements or configurations.
- **platformio.ini**: Configuration file for the PlatformIO environment.

## Installation

1. **Clone the repository**:
   ```bash
   git clone https://github.com/samisauchda/TUW_ITSG_SmartPV
