#pragma once

#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    Logger(const std::string& filename = "", bool console_output = true);
    ~Logger();

    void setLogFile(const std::string& filename);
    void setLogLevel(LogLevel level) { min_level_ = level; }
    void enableConsoleOutput(bool enable) { console_output_ = enable; }

    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

    // Stream operators for easier logging
    Logger& operator<<(const std::string& message);
    Logger& operator<<(LogLevel level);

private:
    std::string getTimestamp() const;
    void log(LogLevel level, const std::string& message);

    std::ofstream log_file_;
    std::string filename_;
    LogLevel min_level_;
    bool console_output_;
    LogLevel current_level_;
};

#endif // LOGGER_H
