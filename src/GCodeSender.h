#ifndef GCODESENDER_H
#define GCODESENDER_H

#include <Arduino.h>
#include <lilka.h>
#include "SerialGrblParser.h"

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Frame;

class GCodeSender {
public:
    GCodeSender(SerialGrblParser& parser);
    
    // Function to send G-code file to GRBL
    void sendGCodeFile(String filename);
    
    // Draw frame around the workspace defined by the frame
    void drawFrame(const Frame& frame);
    
    // Utility functions for parsing and estimating G-code
    Frame inbound(const std::string& command, const Frame& frame, bool isRelative);
    uint32_t estimateCommandTime(const std::string& command);
    String secondsToTime(uint32_t seconds);

private:
    SerialGrblParser& grblParser;
    const int MAX_ERRORS = 10;
    std::vector<std::string> commandList; // List of commands sent to GRBL

    void drawLogs(int lineCount, int totalLines, size_t errors, size_t pauses);
};

#endif // GCODESENDER_H
