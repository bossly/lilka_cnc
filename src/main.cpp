#include <Arduino.h>
#include <lilka.h>
#include "SerialGrblParser.h"
#include "cnc_splash.h"

#define PIN_RX     44
#define PIN_TX     43
#define GRBL_BAUD  115200
#define DEBUG      0

#define GrblSerial Serial2

SerialGrblParser grblParser(GrblSerial);

int x = 0, y = 0, z = 0;
bool isHold = false;
bool isJog = false;
bool isFile = false;
std::string version;
Grbl::MachineState machineState = Grbl::MachineState::Idle;


void setup() {
    lilka::display.setSplash(cnc_splash, cnc_splash_length);
    lilka::begin();

    Serial.begin(9600);
    GrblSerial.begin(GRBL_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

    grblParser.onGCodeAboutToBeSent = [](std::string command) {
        if (DEBUG) {
            Serial.printf("Sending command: %s\n", command.c_str());
        }
    };

    grblParser.onResponseAboutToBeProcessed = [](std::string response) {
        if (DEBUG) {
            Serial.printf("Got response: %s\n", response.c_str());
        }
        x = grblParser.getWorkCoordinate(Grbl::Axis::X);
        y = grblParser.getWorkCoordinate(Grbl::Axis::Y);
        z = grblParser.getWorkCoordinate(Grbl::Axis::Z);
   };

    grblParser.onMachineStateChanged = [](Grbl::MachineState previousState, Grbl::MachineState currentState) {
        machineState = currentState;
        if (DEBUG) {
            Serial.printf("Machine state changed: %s -> %s", String(Grbl::machineStates[static_cast<int>(previousState)]), String(Grbl::machineStates[static_cast<int>(currentState)]));
        }
    };
/*
    grblParser.onPositionUpdated = [](Grbl::MachineState machineState, Grbl::CoordinateMode coordinateMode, const Grbl::Coordinate &coordinate) {
        switch (coordinateMode) {
            case Grbl::CoordinateMode::Work:
                Serial.printf("Work Coordinate - X: %.2f, Y: %.2f, Z: %.2f\n", coordinate[0], coordinate[1], coordinate[2]);
                break;
            case Grbl::CoordinateMode::Machine:
                Serial.printf("Machine Coordinate - X: %.2f, Y: %.2f, Z: %.2f\n", coordinate[0], coordinate[1], coordinate[2]);
                break;
        }
        Serial.printf("Machine state changed: %s", String(Grbl::machineStates[static_cast<int>(machineState)]));
    };
// */
    delay(300);

    // grblParser.sendCommand("G21"); // Set units to mm
    // grblParser.sendCommand("G90"); // Set absolute positioning
    // grblParser.sendCommand("G0 Z5"); // Move Z axis to 5mm
}


void jogControl() {
    lilka::Canvas canvas;

    /**
     * >>> $I
       [VER:1.1h.20190830:]
       [OPT:V,15,128]
     */
    //grblParser.sendCommand("$I");
    // version = grblParser.sendCommandExpectingOk("$I"); // Get GRBL version
    // Serial.printf("GRBL version: %s\n", version.c_str());
    bool ok = grblParser.softReset();
    if (DEBUG) {
        Serial.printf("GRBL soft reset: %s\n", ok ? "OK" : "Failed");
    }

    // put your main code here, to run repeatedly:
    while (isJog) {
        lilka::State state = lilka::controller.getState();
        int feedRate = 1000; //grblParser.getCurrentFeedRate();
        int distance = 5; // mm

        if (state.up.justPressed) {
            y += distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::Y, y}}); // Jog Y axis
        }

        if (state.down.justPressed) {
            y -= distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::Y, y}}); // Jog Y axis
        }

        if (state.left.justPressed) {
            x -= distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::X, x}}); // Jog Y axis
        }

        if (state.right.justPressed) {
            x += distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::X, x}}); // Jog Y axis
        }

        if (state.c.justPressed) {
            ok = grblParser.softReset();
        }

        if (state.b.justPressed) {
            isJog = false;
        }

        // Display X, Y, Z and Hold status on canvas
        canvas.fillScreen(lilka::colors::Black);
        canvas.setTextColor(lilka::colors::White);
        canvas.setCursor(5, 50);
        canvas.print("X: " + String(x) + " Y: " + String(y) + " Z: " + String(z));
        canvas.setCursor(5, 70);

        machineState = grblParser.machineState();

        // Display machine state
        if (machineState == Grbl::MachineState::Door || machineState == Grbl::MachineState::Hold || machineState == Grbl::MachineState::Alarm) {
            canvas.setTextColor(lilka::colors::Red);
            canvas.print("Status: " + String(Grbl::machineStates[static_cast<int>(machineState)]));
        } else {
            canvas.setTextColor(lilka::colors::Green);
            canvas.print("Status: " + String(Grbl::machineStates[static_cast<int>(machineState)]));
        }

        canvas.setTextColor(lilka::colors::White);
        canvas.setCursor(5, 90);
        canvas.print("Feed rate: " + String(feedRate) + " mm/min");
        canvas.setCursor(5, 110);
        canvas.print("Distance: " + String(distance) + " mm");

        canvas.setCursor(5, 150);
        canvas.print("Press B to exit");
        canvas.setCursor(5, 170);
        canvas.print("Press C to reset");
        grblParser.update();

        lilka::display.drawCanvas(&canvas);
    }
}

uint32_t estimateCommandTime(const std::string& command) {
    uint32_t baseTime = 100; // Base time in milliseconds for any command
    
    // Parse G-code command
    if (command.find("G0") != std::string::npos || command.find("G1") != std::string::npos) {
        float feedRate = 0;
        size_t fPos = command.find('F');
        if (fPos != std::string::npos) {
            // Extract feed rate value after F
            feedRate = atof(command.c_str() + fPos + 1);
        } else {
            feedRate = grblParser.getCurrentFeedRate();
        }

        // If no feed rate found or it's 0, use a default
        if (feedRate <= 0) feedRate = 1000;

        // Estimate movement distance (simplified)
        float x = 0, y = 0;
        size_t xPos = command.find('X');
        size_t yPos = command.find('Y');

        if (xPos != std::string::npos) x = abs(atof(command.c_str() + xPos + 1));
        if (yPos != std::string::npos) y = abs(atof(command.c_str() + yPos + 1));

        // Calculate diagonal distance
        float distance = sqrt(x*x + y*y);

        // Calculate time in milliseconds: (distance / feedrate) * 60000
        // feedrate is in mm/min, so multiply by 60000 to get ms
        if (distance > 0) {
            return baseTime + (uint32_t)((distance / feedRate) * 60000);
        }
    }

    return baseTime; // Return base time for non-movement commands
}

String secondsToTime(uint32_t seconds) {
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, secs);
    return String(buffer);
}

void sendGCodeFile(String filename, lilka::Canvas& canvas) {
    FILE* file = fopen(String("/sd/" + String(filename)).c_str(), "r");
    if (!file) {
        Serial.println("Не вдалося відкрити файл");
        return;
    }

    // Count total non-comment lines first
    int totalLines = 0;
    uint32_t totalTime = 0;
    char countLine[256];
    while (fgets(countLine, sizeof(countLine), file)) {
        if (countLine[0] != ';') {
            totalLines++;
            totalTime += estimateCommandTime(countLine);
        }
    }
    rewind(file);  // Reset file pointer to beginning

    // Display file info screen
    canvas.fillScreen(lilka::colors::Black);
    canvas.setTextColor(lilka::colors::White);
    canvas.setCursor(5, 30);
    canvas.print("File: " + String(filename));
    canvas.setCursor(5, 50);
    canvas.print("Total lines: " + String(totalLines));
    canvas.setCursor(5, 70);
    canvas.print("Total time: " + secondsToTime(totalTime/1000));
    canvas.setCursor(5, 90);
    canvas.print("Press A to start");
    canvas.setCursor(5, 110);
    canvas.print("Press B to cancel");
    lilka::display.drawCanvas(&canvas);

    // Wait for user confirmation
    while (true) {
        lilka::State state = lilka::controller.getState();
        if (state.b.justPressed) {
            fclose(file);
            return;
        }
        if (state.a.justPressed) {
            break;
        }
        delay(10);
    }

    if (grblParser.softReset()) {
        if (DEBUG) {
            Serial.println("GRBL скинуто, починаємо виконання файлу...");
        }
    } else if (DEBUG) {
        Serial.println("Не вдалося скинути GRBL");
    }
    
    char line[256];
    int lineCount = 0;
    int errors = 0;
    int totalErrors = 0;
    const int MAX_ERRORS = 10;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] != ';') {  // Skip comments
            bool ok = grblParser.sendCommandExpectingOk(std::string(line));

            while (!ok && errors < MAX_ERRORS) {
                errors++;
                uint32_t wait = 100 * errors * errors;
                if (DEBUG) {
                    Serial.println("Помилка при надсиланні команди, повторюємо... " + String(wait) + "ms");
                }
                delay(wait);
                ok = grblParser.sendCommandExpectingOk(std::string(line));
            }

            if (errors >= MAX_ERRORS) {
                int percentage = (lineCount * 100) / totalLines;
                if (DEBUG) {
                    Serial.println("Помилка при надсиланні команди... " + String(percentage) + "%");
                }
                break;
            }

            if (errors > 0) {
                if (DEBUG) {
                    Serial.println("Команда надіслана успішно після " + String(errors) + " спроб");
                }   
            }

            totalErrors += errors;
            errors = 0;
            lineCount++;

            // Draw execution status on canvas
            canvas.fillScreen(lilka::colors::Black);
            canvas.setTextColor(lilka::colors::White);
            
            // File progress
            canvas.setCursor(5, 30);
            int percentage = (lineCount * 100) / totalLines;
            canvas.print("Progress: " + String(percentage) + "%");
            
            // Line count
            canvas.setCursor(5, 50);
            canvas.print("Lines: " + String(lineCount) + "/" + String(totalLines));
            
            // Current position
            canvas.setCursor(5, 70);
            canvas.print("X: " + String(grblParser.getMachineCoordinate(Grbl::Axis::X)));
            canvas.setCursor(5, 90);
            canvas.print("Y: " + String(grblParser.getMachineCoordinate(Grbl::Axis::Y)));

            // Machine state
            canvas.setCursor(5, 130);
            Grbl::MachineState state = grblParser.machineState();
            if (state == Grbl::MachineState::Run) {
                canvas.setTextColor(lilka::colors::Green);
            } else if (state == Grbl::MachineState::Hold || state == Grbl::MachineState::Alarm) {
                canvas.setTextColor(lilka::colors::Red);
            } else {
                canvas.setTextColor(lilka::colors::White);
            }
            canvas.print("Status: " + String(Grbl::machineStates[static_cast<int>(state)]));

            lilka::display.drawCanvas(&canvas);           
        }
    }

    fclose(file);

    if (DEBUG) {
        Serial.println("Виконання файлу завершено!");
        Serial.println("Загальна кількість помилок: " + String(totalErrors));
    }

    // Check if GRBL is still running
    while (true) {
        // Wait for GRBL to finish processing
        delay(100);
        grblParser.update();
        machineState = grblParser.machineState();

        // Display X, Y, Z and Hold status on canvas
        canvas.fillScreen(lilka::colors::Black);
        canvas.setTextColor(lilka::colors::White);
        canvas.setCursor(5, 50);
        canvas.print("X: " + String(x) + " Y: " + String(y) + " Z: " + String(z));

        // Display machine state
        if (machineState == Grbl::MachineState::Door || machineState == Grbl::MachineState::Hold || machineState == Grbl::MachineState::Alarm) {
            canvas.setTextColor(lilka::colors::Red);
        } else {
            canvas.setTextColor(lilka::colors::Green);
        }
        canvas.setCursor(5, 70);
        canvas.print("Status: " + String(Grbl::machineStates[static_cast<int>(machineState)]));

        if (machineState == Grbl::MachineState::Idle) {
            canvas.setTextColor(lilka::colors::White);
            canvas.setCursor(5, 170);
            canvas.print("Press B to close");
        }

        lilka::State state = lilka::controller.getState();

        if (state.a.justPressed) {
            break;
        }

        grblParser.update();
        lilka::display.drawCanvas(&canvas);
    }
}

void fileSelection() {
    // Handle file selection
    lilka::Canvas canvas;
    lilka::Menu main("Select File");
    size_t entriesCount = lilka::fileutils.getEntryCount(&SD, "/");

    // display list of files
    lilka::Entry entries[entriesCount];
    lilka::fileutils.listDir(&SD, "/", entries);
    main.addItem(".. повернутись");
    for (size_t i = 0; i < entriesCount; i++) {
        if (entries[i].name[0] != '.') {
            main.addItem(entries[i].name);
        }
    }

    while (!main.isFinished()) {
        main.update();
        main.draw(&canvas);
        lilka::display.drawCanvas(&canvas);
    }
    
    int index = main.getCursor();
    if (index == 0) {
        isFile = false;
        return;
    }

    lilka::MenuItem item;
    main.getItem(index, &item);
    sendGCodeFile(item.title, canvas);
    isFile = false;
}

void loop() {
    if (isJog) {
        jogControl();
    } else if (isFile) {
        fileSelection();
    } else {
        lilka::Canvas canvas;
        lilka::Menu main("GRBl Controller");
        main.addItem("Почати");
        main.addItem("Вибрати файл");

        while (!main.isFinished()) {
            main.update();
            main.draw(&canvas);
            lilka::display.drawCanvas(&canvas);
        }

        int index = main.getCursor();

        if (index == 0) {
            isJog = true;
        } else if (index == 1) {
            isFile = true;
        }

        // Handle menu item selection
        lilka::MenuItem item;
        main.getItem(index, &item);
        // Serial.println(String("Ви обрали пункт ") + item.title);
    }
}
