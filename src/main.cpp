#include <Arduino.h>
#include <lilka.h>
#include "SerialGrblParser.h"
#include "cnc_splash.h"
#include "GCodeSender.h"
#include "FileSelect.h"
#include "JogControl.h"
#include <regex> // Add for regex parsing

#define PIN_RX     44
#define PIN_TX     43
#define GRBL_BAUD  115200
#define DEBUG      1

#define GrblSerial Serial2

SerialGrblParser grblParser(GrblSerial);
FileSelect fileSelector(grblParser);
JogControl jogControl(grblParser);

int x = 0, y = 0, z = 0;
bool isHold = false;
bool isJog = false;
bool isFile = false;

static unsigned long startTime = 0; // Start time for command timestamping
Grbl::MachineState machineState = Grbl::MachineState::Idle;

std::string grblVersion = "Unknown"; // Store parsed version

void setup() {
    lilka::display.setSplash(cnc_splash, cnc_splash_length);
    lilka::begin();

    Serial.begin(9600);
    GrblSerial.begin(GRBL_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
    startTime = millis();

    // grblParser.onGCodeAboutToBeSent = [](std::string command) {
    //     // Add command to list
    //     unsigned long timestamp = millis() - startTime;
    //     commandList.push_back(std::to_string(timestamp) + ": " + command);
    //     // Serial.printf("Sending command: %s\n", command.c_str());
    // };

    grblParser.onResponseAboutToBeProcessed = [](std::string response) {
        // Serial.printf("Got response: %s\n", response.c_str());
        // add command to list
        // unsigned long timestamp = millis() - startTime;
        // commandList.push_back(std::to_string(timestamp) + ": " + response);

        // Parse [VER:1.1h.20190830:]
        size_t verStart = response.find("[VER:");
        if (verStart != std::string::npos) {
            size_t verEnd = response.find(":", verStart + 5);
            if (verEnd != std::string::npos) {
                std::string verStr = response.substr(verStart + 5, verEnd - (verStart + 5));
                // Extract only the version part before the first dot after 1.1h
                size_t dotPos = verStr.find('.', 4); // after "1.1h"
                if (dotPos != std::string::npos) {
                    grblVersion = "grbl " + verStr.substr(0, dotPos);
                } else {
                    grblVersion = "grbl " + verStr;
                }
            }
        }
    };

    grblParser.softReset(); // Reset GRBL on startup
}

void loop() {
    lilka::Canvas canvas;

    bool ok = grblParser.sendCommandExpectingOk(Grbl::Command::ViewBuildInfo);

    if (!ok) {
        grblVersion = "Unknown"; // Reset version if command fails
    }

    lilka::Menu main("GRBL Controller");

    main.addItem("Почати");
    main.addItem("Вибрати файл");
    //main.addItem("Налаштування");
    main.addActivationButton(lilka::Button::A); // Add activation button for menu selection

    while (!main.isFinished()) {
        main.update();
        main.draw(&canvas);

        canvas.setCursor(38, 150); // FONT_Y / 2
        canvas.setFont(FONT_8x13);
        canvas.setTextColor(lilka::colors::White);
        canvas.setTextSize(1);
        canvas.print("Status: ");
        canvas.print(grblVersion.c_str());

        lilka::display.drawCanvas(&canvas);
    }

    int index = main.getCursor();

    if (index == 0) {
        jogControl.show();
    } else if (index == 1) {
        fileSelector.show();
    }
}
