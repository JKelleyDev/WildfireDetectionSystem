# Wildfire Dectection System 

## Hardware
- [Heltec ESP32 WiFi LoRa V3]( https://heltec.org/project/wifi-lora-32-v3/)
- MQ-9 Sensor
- MQ-135 Sensor
- ESP32 compatable temperature sensor 

## Software 
Must use ESP32 friedly IDE, recomended to use Arduinio IDE 
### Arduino IDE 2.3.4
[Windows]( https://downloads.arduino.cc/arduino-ide/arduino-ide_2.3.4_Windows_64bit.exe )
[Linux](https://downloads.arduino.cc/arduino-ide/arduino-ide_2.3.4_Linux_64bit.AppImage)
[MacOS(Silicon)](https://downloads.arduino.cc/arduino-ide/arduino-ide_2.3.4_macOS_arm64.dmg)
[MacOS(Intel)]( https://downloads.arduino.cc/arduino-ide/arduino-ide_2.3.4_macOS_64bit.dmg)

#### Arduino Setup Instructions 
[Quick Start Guide](https://docs.heltec.org/en/node/esp32/esp32_general_docs/quick_start.html)

## Instructions for Using This Repository with Arduino IDE

Follow these steps to set up a new Arduino sketch and use specific files from this repository:

### 1. Create a New Sketch in Arduino IDE
1. **Open Arduino IDE**: Launch the Arduino IDE on your computer.
2. **Start a New Sketch**: Go to `File > New` (or press `Ctrl+N` / `Cmd+N`) to create a blank sketch.
3. **Save the Sketch**: Go to `File > Save As...`, choose a location on your computer (e.g., `Documents/Arduino/MyProject`), and name your project (e.g., `MyProject`). This will create a folder with that name containing a `.ino` file (e.g., `MyProject.ino`).

### 2. Download Specific Files from This Repository
To use this project, you only need the following files from this repository:
- `SensorNode2.0.ino` – The sensor Arduino sketch file.
- `RecieverNode2.0.ino` – The reciever Arduino sketch file (seperate sketch needed).

Here’s how to get them:
1. **Navigate to the Repository**: Open this GitHub repository in your browser.
2. **Download Individual Files**:
   - Locate each file in the repository file list.
   - Click on the file name. 
   - Click the **Raw** button (or right-click and select "Save Link As...").
   - Save the file to the folder you created in Step 1 (e.g., `Documents/Arduino/MyProject`).
   - Repeat for each file listed above.
3. **Replace the Default Sketch**: If prompted, overwrite the existing `MyProject.ino` with the downloaded `xxxxxxx.ino`.

Alternatively, you can download the entire repository as a ZIP file and manually copy only the specified files:
- Click the green **Code** button > **Download ZIP**.
- Extract the ZIP file, then copy the files into into your project folder.

### 3. Open and Verify in Arduino IDE
1. **Open the Project**: In Arduino IDE, go to `File > Open`, navigate to your project folder (e.g., `Documents/Arduino/MyProject`), and select `xxxxxx.ino`.
2. **Compile and Upload**: Connect your Arduino board, select the correct board and port under `Tools`, then click **Verify** (`Ctrl+R`) and **Upload** (`Ctrl+U`) to test the sketch.





