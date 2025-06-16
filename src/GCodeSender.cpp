#include "GCodeSender.h"
#include <cmath>

// Constructor to initialize with the GRBL parser
GCodeSender::GCodeSender(SerialGrblParser& parser) : grblParser(parser) {
}

Frame GCodeSender::inbound(const std::string& command, const Frame& frame, bool isRelative) {
    Frame newFrame = frame;

    // Parse G-code command
    if (command.find("G0") != std::string::npos || command.find("G1") != std::string::npos) {
        float x = 0, y = 0;
        size_t xPos = command.find('X');
        size_t yPos = command.find('Y');

        if (xPos != std::string::npos) {
            x = atof(command.c_str() + xPos + 1);
            if (isRelative) {
                x += newFrame.x;
            }
        }
        if (yPos != std::string::npos) {
            y = atof(command.c_str() + yPos + 1);
            if (isRelative) {
                y += newFrame.y;
            }
        }

        // Calculate width and height
        if (x > newFrame.width) newFrame.width = x;
        if (y > newFrame.height) newFrame.height = y;
    } else if (command.find("G2") != std::string::npos) {
        // Handle circular interpolation commands
        float i = 0, j = 0;
        size_t iPos = command.find('I');
        size_t jPos = command.find('J');
    } else if (command.find("G3") != std::string::npos) {
        // Handle circular interpolation commands
        float i = 0, j = 0;
        size_t iPos = command.find('I');
        size_t jPos = command.find('J');
    }
    return newFrame;
}


Grbl::Point drawCommand(lilka::Canvas& canvas, const std::string& command, Grbl::Point position, bool isRelative) {
    
    bool rapid = command.find("G0") != std::string::npos;
    bool linear = command.find("G1") != std::string::npos;
    uint16_t color = lilka::colors::White; // Default color for drawing

    // Draw the command on the canvas
    if (rapid || linear) {
        Grbl::Point destination = position; // Start from the current position
        size_t xPos = command.find('X');
        size_t yPos = command.find('Y');
        size_t signalPos = command.find('S'); // 0 - 1000

        if (xPos != std::string::npos) {
            destination.first = atof(command.c_str() + xPos + 1);
            if (isRelative) {
                destination.first += position.first;
            }
        }

        if (yPos != std::string::npos) {
            destination.second = atof(command.c_str() + yPos + 1);
            if (isRelative) {
                destination.second += position.second;
            }
        }

        // set black color for rapid, green for linear
        if (rapid) {
            color = lilka::colors::Black;
        } else if (linear) {
            color = lilka::colors::Green;
        }

        // If S parameter is present, set the speed (0-1000)
        if (signalPos != std::string::npos) {
            int signal = atoi(command.c_str() + signalPos + 1);
            if (signal >= 0 && signal <= 1000) {
                // Map signal to color intensity (for demonstration purposes)
                uint8_t intensity = map(signal, 0, 1000, 0, 255);
                color = ((intensity >> 3) << 11) | ((intensity >> 2) << 5) | (intensity >> 3); // Convert grayscale to RGB565 format
            }
        }

        // Draw a line from the current position to the destination
        canvas.drawLine(position.first, position.second, destination.first, destination.second, color);
        return destination; // Return the new position
    } else if (command.find("G2") != std::string::npos) { // cw
        // Handle G2 command (clockwise arc)
        size_t iPos = command.find('I');
        size_t jPos = command.find('J');

        float i = 0, j = 0;

        if (iPos != std::string::npos) i = abs(atof(command.c_str() + iPos + 1));
        if (jPos != std::string::npos) j = abs(atof(command.c_str() + jPos + 1));

        // Draw an arc (simplified, just a circle for demonstration)
        float radius = sqrt(i * i + j * j);
        float angle = 0; // Angle for arc drawing
        float arcSteps = 36; // Number of steps for the arc
        float angleStep = 360.0 / arcSteps;

        for (int step = 0; step < arcSteps; step++) {
            float angleRad = (angle * M_PI) / 180.0;
            float x = position.first + radius * cos(angleRad);
            float y = position.second + radius * sin(angleRad);
            canvas.drawPixel(x, y, lilka::colors::Green);
            angle += angleStep;
        }
        return {position.first + radius * cos(0), position.second + radius * sin(0)}; // Return the end position of the arc
    } else if (command.find("G3") != std::string::npos) { //ccw
        // Handle G3 command (counter-clockwise arc)
        size_t iPos = command.find('I');
        size_t jPos = command.find('J');
        float i = 0, j = 0;

        if (iPos != std::string::npos) i = abs(atof(command.c_str() + iPos + 1));
        if (jPos != std::string::npos) j = abs(atof(command.c_str() + jPos + 1));

        // Draw an arc (simplified, just a circle for demonstration)
        float radius = sqrt(i * i + j * j);
        float angle = 0; // Angle for arc drawing
        float arcSteps = 36; // Number of steps for the arc
        float angleStep = 360.0 / arcSteps;

        for (int step = 0; step < arcSteps; step++) {
            float angleRad = (angle * M_PI) / 180.0;
            float x = position.first + radius * cos(angleRad);
            float y = position.second + radius * sin(angleRad);
            canvas.drawPixel(x, y, lilka::colors::Red);
            angle += angleStep;
        }
        return {position.first + radius * cos(0), position.second + radius * sin(0)}; // Return the end position of the arc
    }
}

uint32_t GCodeSender::estimateCommandTime(const std::string& command) {
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
        float distance = sqrt(x * x + y * y);

        // Calculate time in milliseconds: (distance / feedrate) * 60000
        // feedrate is in mm/min, so multiply by 60000 to get ms
        if (distance > 0) {
            return baseTime + (uint32_t)((distance / feedRate) * 60000);
        }
    }

    return baseTime; // Return base time for non-movement commands
}

String GCodeSender::secondsToTime(uint32_t seconds) {
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours, minutes, secs);
    return String(buffer);
}

void GCodeSender::drawFrame(const Frame& frame) {
    int speed = 2000;
    int safeHeight = 5;

    // Move to the starting position
    bool ok = grblParser.linearInterpolationPositioning(
        speed,
        {std::make_pair(Grbl::Axis::X, frame.x),
         std::make_pair(Grbl::Axis::Y, frame.y),
         std::make_pair(Grbl::Axis::Z, safeHeight)}
    );

    // Move to the top right corner
    ok = grblParser.linearInterpolationPositioning(
        speed,
        {std::make_pair(Grbl::Axis::X, frame.x + frame.width),
         std::make_pair(Grbl::Axis::Y, frame.y),
         std::make_pair(Grbl::Axis::Z, safeHeight)}
    );

    // Move to the bottom right corner
    ok = grblParser.linearInterpolationPositioning(
        speed,
        {std::make_pair(Grbl::Axis::X, frame.x + frame.width),
         std::make_pair(Grbl::Axis::Y, frame.y + frame.height),
         std::make_pair(Grbl::Axis::Z, safeHeight)}
    );

    // Move to the bottom left corner
    ok = grblParser.linearInterpolationPositioning(
        speed,
        {std::make_pair(Grbl::Axis::X, frame.x),
         std::make_pair(Grbl::Axis::Y, frame.y + frame.height),
         std::make_pair(Grbl::Axis::Z, safeHeight)}
    );

    // Move back to the starting position
    ok = grblParser.linearInterpolationPositioning(
        speed,
        {std::make_pair(Grbl::Axis::X, frame.x),
         std::make_pair(Grbl::Axis::Y, frame.y),
         std::make_pair(Grbl::Axis::Z, safeHeight)}
    );
}

void GCodeSender::drawLogs(int lineCount, int totalLines, size_t errors, size_t pauses) {
    lilka::Canvas canvas;

    // Draw execution status on canvas
    canvas.fillScreen(lilka::colors::Black);
    canvas.setFont(FONT_5x7);
    // canvas.setTextSize(1);
    canvas.setTextColor(lilka::colors::White);

    // File progress
    canvas.setCursor(50, 10);
    int percentage = (lineCount * 100) / totalLines;
    canvas.print("Progress: " + String(percentage) + "%");

    // Line count
    canvas.setCursor(160, 10);
    canvas.print("Lines: " + String(lineCount) + "/" + String(totalLines));

    // Display list of commands sent to GRBL
    canvas.setCursor(10, 25);
    canvas.print("Commands sent to GRBL:\n");

    size_t maxCommands = 16;

    if (commandList.size() > maxCommands) {
        commandList.erase(commandList.begin(), commandList.end() - maxCommands);
    }

    size_t line = 35;
    size_t lineHeight = 12; // Set line height for command display

    // Display each command
    for (size_t i = 0; i < commandList.size(); i++) {
        canvas.setCursor(10, line + i * lineHeight);
        canvas.print(commandList[i].c_str());
    }

    canvas.setCursor(30, line + commandList.size() * lineHeight);
    canvas.setTextColor(lilka::colors::Red);
    canvas.print("Errors: " + String(errors));

    canvas.setCursor(100, line + commandList.size() * lineHeight);
    canvas.setTextColor(lilka::colors::Yellow);
    canvas.print("Buffering: " + String(pauses));

    canvas.setCursor(180, line + commandList.size() * lineHeight);
    canvas.setTextColor(lilka::colors::Green);
    canvas.print("Status: " + String(Grbl::machineStates[static_cast<int>(grblParser.machineState())]));

    grblParser.update();
    lilka::display.drawCanvas(&canvas);
}

// main function to send G-code file to GRBL

uint16_t* flipVertically(uint16_t* framebuffer, int16_t w, int16_t h)  // Flip the framebuffer vertically
{
  for (int16_t y = 0; y < h / 2; ++y)
  {
    uint16_t* rowTop = framebuffer + y * w;
    uint16_t* rowBottom = framebuffer + (h - 1 - y) * w;

    for (int16_t x = 0; x < w; ++x)
    {
      uint16_t tmp = rowTop[x];
      rowTop[x] = rowBottom[x];
      rowBottom[x] = tmp;
    }
  }

  return framebuffer;  // Return the modified framebuffer
}

void GCodeSender::sendGCodeFile(String filename) {
    lilka::Canvas canvas;

    FILE* file = fopen(String("/sd/" + String(filename)).c_str(), "r");
    if (!file) {
        Serial.println("Не вдалося відкрити файл");
        return;
    }

    static unsigned long startTime = millis(); // Start time for command timestamping
    size_t errors = 0;

    grblParser.onGCodeAboutToBeSent = [this](std::string command) {
        // Add command to list
        unsigned long timestamp = millis() - startTime;
        commandList.push_back(std::to_string(timestamp / 100) + " s: " + command);
        // Serial.printf("Sending command: %s\n", command.c_str());
    };

    grblParser.onResponseAboutToBeProcessed = [this, &errors](std::string response) {
        // Serial.printf("Got response: %s\n", response.c_str());
        // add command to list
        unsigned long timestamp = millis() - startTime;

        // If response is an error, add it to the command list
        if (response.find("error") != std::string::npos) {
            commandList.push_back(std::to_string(timestamp / 100) + " e: " + response);
            errors++;
        } else {
            commandList.push_back(std::to_string(timestamp / 100) + " r: " + response);
        }
    };

    grblParser.onMachineStateChanged = [this](Grbl::MachineState previous, Grbl::MachineState state) {
        // Serial.printf("Machine state changed: %s\n", Grbl::machineStateToString(state).c_str());
        unsigned long timestamp = millis() - startTime;
        commandList.push_back(std::to_string(timestamp / 100) + " m: " + Grbl::machineStates[static_cast<int>(state)]);
    };

    canvas.fillScreen(lilka::colors::Black);
    canvas.setFont(FONT_5x7);
    canvas.setTextSize(1);
    canvas.setTextColor(lilka::colors::White);
    canvas.setCursor(10, 10);
    canvas.print("G-code file: " + filename);

    // Count total non-comment lines first
    int totalLines = 0;
    char line[256];

    uint32_t totalTime = 0;
    Frame frame = {0, 0, 0, 0}; // Initialize frame

    lilka::Canvas previewCanvas;
    previewCanvas.fillScreen(lilka::colors::Black);
    bool isRelative = true; // Track if the positioning is relative
    Grbl::Point position = {0, 0}; // Starting position for drawing commands

    while (fgets(line, sizeof(line), file)) {

        const std::string& command = line;

        if (line[0] != ';') {
            // Draw the command on the canvas
            if (command.find("G90") != std::string::npos) {
                // Absolute positioning
                isRelative = false;
            } else if (command.find("G91") != std::string::npos) {
                // Relative positioning
                isRelative = true;
            }

            totalLines++;
            totalTime += estimateCommandTime(line);
            frame = inbound(line, frame, isRelative);
            position = drawCommand(previewCanvas, command, position, isRelative);
        }
    }

    // center preview canvas on the main canvas
    int offsetX = (lilka::display.width() - frame.width) / 2;
    int offsetY = -(lilka::display.height() - frame.height) / 2;
    uint16_t* buffer = flipVertically(previewCanvas.getFramebuffer(), previewCanvas.width(), previewCanvas.height());
    // canvas.draw16bitRGBBitmap(offsetX, offsetY, buffer, previewCanvas.width(), previewCanvas.height());
    canvas.draw16bitRGBBitmap(offsetX, offsetY, buffer, previewCanvas.width(), previewCanvas.height());

    rewind(file); // Reset file pointer to beginning
    lilka::display.drawCanvas(&canvas);

    bool isFrame = false;

    // Wait for user confirmation
    while (true) {
        lilka::State state = lilka::controller.getState();
        if (state.b.justPressed) {
            fclose(file);
            return;
        }
        if (state.start.justPressed) {
            break;
        }
        if (state.a.justPressed) {
            isFrame = true;
            break;
        }
        delay(10);
    }

    if (grblParser.softReset()) {
        delay(1000); // Wait for GRBL to reset
    }

    // Clear the command list
    commandList.clear();

    int lineCount = 0;
    int pauses = 0;
    bool stop = false;

    if (isFrame) {
        // Draw frame around the workspace
        drawFrame(frame);
    } else {
        // Handle non-frame-specific logic
        while (fgets(line, sizeof(line), file)) {
            if (line[0] != ';') { // Skip comments
                bool ok = grblParser.sendCommandExpectingOk(std::string(line));

                if (!ok) {
                    pauses++;
                    unsigned long timestamp = millis() - startTime;
                    commandList.push_back(std::to_string(timestamp / 100) + " w: wait 100 ms");

                    delay(100); // Wait for 100 ms before continuing
                }

                lineCount++;
                drawLogs(lineCount, totalLines, errors, pauses);

                lilka::State state = lilka::controller.getState();

                if (state.a.justPressed) {
                    stop = true; // Stop the loop
                    break;
                }
            }
        }
    }

    fclose(file);

    // Get global variable references
    extern Grbl::MachineState machineState;
    extern bool isFile;

    // Wait for GRBL to finish processing
    while (!stop) {
        grblParser.update();
        machineState = grblParser.machineState();
        drawLogs(lineCount, totalLines, errors, pauses);

        lilka::State state = lilka::controller.getState();
        if (state.a.justPressed) {
            isFile = false; // Exit jog mode
            stop = true; // Stop the loop
        }
    }

    // Clear the canvas when exiting jog mode
    grblParser.onGCodeAboutToBeSent = nullptr;
    grblParser.onResponseAboutToBeProcessed = nullptr;
}
