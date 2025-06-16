#include "JogControl.h"

// Constructor to initialize with the GRBL parser
JogControl::JogControl(SerialGrblParser& parser) : grblParser(parser) {
    // Initialize coordinates
}

void JogControl::show() {
    static unsigned long startTime = millis(); // Start time for command timestamping
    bool ok = grblParser.softReset();

    grblParser.onGCodeAboutToBeSent = [this](std::string command) {
        // Add command to list
        unsigned long timestamp = millis() - startTime;
        commandList.push_back(std::to_string(timestamp / 100) + ": " + command);
        // Serial.printf("Sending command: %s\n", command.c_str());
    };

    grblParser.onResponseAboutToBeProcessed = [this](std::string response) {
        // Serial.printf("Got response: %s\n", response.c_str());
        // add command to list
        unsigned long timestamp = millis() - startTime;
        commandList.push_back(std::to_string(timestamp / 100) + ": " + response);
    };

    while (true) {
        bool ok;
        lilka::State state = lilka::controller.getState();

        if (state.up.justPressed) {
            y += distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::Y, y}});
        }
        if (state.down.justPressed) {
            y -= distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::Y, y}});
        }
        if (state.left.justPressed) {
            x -= distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::X, x}});
        }
        if (state.right.justPressed) {
            x += distance;
            ok = grblParser.jog(feedRate, {{Grbl::Axis::X, x}});
        }
        if (state.c.justPressed) {
            ok = grblParser.softReset();
        }
        if (state.b.justPressed) {
            break; // Exit jog mode
        }
        
        updateDisplay();
        grblParser.update();
    }

    // Clear the canvas when exiting jog mode
    grblParser.onGCodeAboutToBeSent = nullptr;
    grblParser.onResponseAboutToBeProcessed = nullptr;
}

void JogControl::updateDisplay() {
    lilka::Canvas canvas;

    canvas.fillScreen(lilka::colors::Black);
    canvas.setTextColor(lilka::colors::White);
    canvas.setFont(FONT_6x12);
    canvas.setCursor(30, 10);
    canvas.print("X: " + String(x) + " Y: " + String(y) + " Z: " + String(z));
    canvas.setCursor(120, 10);

    machineState = grblParser.machineState();

    if (machineState == Grbl::MachineState::Door || machineState == Grbl::MachineState::Hold ||
        machineState == Grbl::MachineState::Alarm) {
        canvas.setTextColor(lilka::colors::Red);
    } else {
        canvas.setTextColor(lilka::colors::Green);
    }

    canvas.print("Status: " + String(Grbl::machineStates[static_cast<int>(machineState)]));

    // Display list of commands sent to GRBL
    canvas.setTextColor(lilka::colors::White);
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

    lilka::display.drawCanvas(&canvas);
}