#ifndef FILESELECT_H
#define FILESELECT_H

#include <Arduino.h>
#include <lilka.h>
#include "GCodeSender.h"

class FileSelect {
public:
    FileSelect(SerialGrblParser& parser);
    
    // Main function to handle file selection and processing
    void show();

private:
    GCodeSender gcodeSender;
};

#endif // FILESELECT_H
