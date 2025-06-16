#include <lilka.h>
#include "SerialGrblParser.h"

class JogControl {
public:
    JogControl(SerialGrblParser& parser);

    // Main function to handle jogging and processing
    void show();

private:
    void updateDisplay();

    SerialGrblParser& grblParser;
    std::vector<std::string> commandList; // List of commands sent to GRBL

    int x = 0, y = 0, z = 0;
    int feedRate = 1000; // Default feed rate in mm/min
    int distance = 5; // Default jog distance

    Grbl::MachineState machineState = Grbl::MachineState::Idle;
};
