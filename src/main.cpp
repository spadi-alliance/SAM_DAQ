#include <QApplication>
#include <QTimer>
#include "MainWindow.h"
#include "TApplication.h"
#include "TSystem.h"
#include "CLIInterface.h"
#include <TROOT.h>
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char *argv[]) {

    // Check for flags BEFORE creating TApplication
    bool batchMode = false;
    bool multithreaded = false;  // Default: single-threaded
    bool hasCLIArgs = false;
    
    for (int i = 1; i < argc; ++i) {
        QString arg = QString(argv[i]);
        if (arg == "-b") {
            batchMode = true;
        }
        else if (arg == "-m") {
            multithreaded = true;
        }
        // Auto-detect CLI mode for script and help arguments
        else if (arg == "--script" || arg == "--help" || arg == "-h" ||
                 arg == "--ip" || arg == "--output-dir" || arg == "--output-file") {
            batchMode = true;
            hasCLIArgs = true;
        }
    }

    // If we have CLI arguments, force batch mode
    if (hasCLIArgs) {
        batchMode = true;
    }

    // Print threading mode
    std::cout << "SAM_DAQ starting in " 
              << (multithreaded ? "multithreaded" : "single-threaded") 
              << " mode" << std::endl;

    if (batchMode) {
        // Create filtered arguments for both ROOT and CLI (remove processed flags)
        std::vector<std::string> root_args;
        std::vector<std::string> cli_args;
        root_args.push_back(argv[0]); // program name
        cli_args.push_back(argv[0]); // program name
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            // Skip flags that are handled by main.cpp
            if (arg != "-b" && arg != "-m") {
                cli_args.push_back(arg);
            }
            // Skip CLI-specific arguments that ROOT might interpret
            if (arg != "--help" && arg != "-h" &&
                arg != "--ip" && arg != "--output-dir" && arg != "--output-file" &&
                arg != "-b" && arg != "-m") {
                root_args.push_back(arg);
            } else if (arg == "--ip" || arg == "--output-dir" || arg == "--output-file") {
                // Skip the argument value too
                if (i + 1 < argc) i++;
            }
            // Note: --script is intentionally NOT filtered out here because CLI needs it
        }
        
        // Convert back to char** for TApplication
        int root_argc = root_args.size();
        char** root_argv = new char*[root_argc];
        for (int i = 0; i < root_argc; ++i) {
            root_argv[i] = const_cast<char*>(root_args[i].c_str());
        }
        
        // Convert back to char** for CLIInterface
        int cli_argc = cli_args.size();
        char** cli_argv = new char*[cli_argc];
        for (int i = 0; i < cli_argc; ++i) {
            cli_argv[i] = const_cast<char*>(cli_args[i].c_str());
        }
        
        // Set ROOT to batch mode with filtered arguments
        TApplication app("uno", &root_argc, root_argv);
        gROOT->SetBatch(kTRUE);

        CLIInterface cli;
        int result = cli.run(cli_argc, cli_argv); // Pass filtered arguments to CLI
        
        delete[] root_argv;
        delete[] cli_argv;
        return result;
    } else {
        TApplication app("uno", &argc, argv); // ROOT application

        QApplication myApp(argc, argv);

        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, []() { gSystem->ProcessEvents(); });
        timer.setSingleShot(false);
        timer.start(4);

        MainWindow mainWindow(multithreaded); // Pass threading flag to MainWindow
        mainWindow.show();

        return myApp.exec();
    }
}