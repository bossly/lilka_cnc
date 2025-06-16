#include "FileSelect.h"

// Constructor to initialize with the GRBL parser
FileSelect::FileSelect(SerialGrblParser& parser) : gcodeSender(parser) {
    // initialize any necessary variables or settings here
}

void FileSelect::show() {
    // Handle file selection
    lilka::Canvas canvas;
    lilka::Menu main("Select File");
    main.addActivationButton(lilka::Button::A); // Add activation button for menu selection
    main.addActivationButton(lilka::Button::B); // Add activation button for menu selection
    String path = "/"; // Start from the root directory

    while (true) {
        size_t entriesCount = lilka::fileutils.getEntryCount(&SD, path.c_str());

        // display list of files
        lilka::Entry entries[entriesCount] = {};
        lilka::fileutils.listDir(&SD, path.c_str(), entries);
        lilka::State state;

        for (size_t i = 0; i < entriesCount; i++) {
            // Add each file or directory to the menu
            if (entries[i].type == lilka::EntryType::ENT_FILE) {
                main.addItem(entries[i].name, nullptr, lilka::colors::Blue, "");
            } else if (entries[i].type == lilka::EntryType::ENT_DIRECTORY) {
                main.addItem(entries[i].name, nullptr, lilka::colors::Green, "");
            }
        }

        while (!main.isFinished()) {
            main.update();
            main.draw(&canvas);
            lilka::display.drawCanvas(&canvas);
        }

        if (main.getButton() == lilka::Button::B) {
            // If the user selected "..", go back to the previous directory
            if (path != "/") {
                // warn: there is a bug in the original code that does not correctly handle the root directory.
                // path = lilka::fileutils.getParentDirectory(path);

                // Remove the last part of the path
                String cleanPath = path;
                
                // Remove trailing '/' if it exists
                if (cleanPath.endsWith("/")) {
                    cleanPath.remove(cleanPath.length() - 1);
                }
                
                // Find the last '/'
                int lastSlash = cleanPath.lastIndexOf('/');

                if (lastSlash == -1) {
                    // No '/' found, assume it's at the top level
                    path = "/";
                }
                
                // Return the substring up to the last '/'
                path = cleanPath.substring(0, lastSlash+1); // Include the last slash to keep it a directory
                main.clearItems(); // Clear the menu items for the next iteration
            } else { // If already at root
                break; // Go back to the previous menu
            }
        } else {
            // Get the selected index
            int index = main.getCursor();
            lilka::Entry selected = entries[index];

            if (selected.type == lilka::EntryType::ENT_DIRECTORY) {
                // If it's a directory, navigate into it
                path = lilka::fileutils.joinPath(path, selected.name);
                main.clearItems(); // Clear the menu items for the next iteration
            } else {
                gcodeSender.sendGCodeFile(selected.name);
            }
        }
    }
}
