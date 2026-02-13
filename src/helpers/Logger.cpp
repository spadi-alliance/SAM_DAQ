#include "Logger.h"
#include <filesystem>

Logger::Logger(const std::string& filename, bool console_output)
    : filename_(filename), min_level_(LogLevel::INFO), console_output_(console_output), current_level_(LogLevel::INFO)
{
    if (!filename.empty()) {
        setLogFile(filename);
    }
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::setLogFile(const std::string& filename) {
    filename_ = filename;

    // Create directory if it doesn't exist
    std::filesystem::path filePath(filename);
    std::filesystem::path dirPath = filePath.parent_path();
    if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
        std::filesystem::create_directories(dirPath);
    }

    if (log_file_.is_open()) {
        log_file_.close();
    }

    log_file_.open(filename, std::ios::out | std::ios::app);
    if (!log_file_) {
        std::cerr << "Warning: Cannot open log file: " << filename << std::endl;
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

Logger& Logger::operator<<(const std::string& message) {
    log(current_level_, message);
    return *this;
}

Logger& Logger::operator<<(LogLevel level) {
    current_level_ = level;
    return *this;
}

std::string Logger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < min_level_) {
        return;
    }

    std::string level_str;
    switch (level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO: level_str = "INFO"; break;
        case LogLevel::WARNING: level_str = "WARNING"; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
    }

    std::string log_message = "[" + getTimestamp() + "] [" + level_str + "] " + message;

    if (console_output_) {
        (level >= LogLevel::WARNING ? std::cerr : std::cout) << log_message << std::endl;
    }

    if (log_file_.is_open()) {
        log_file_ << log_message << std::endl;
        log_file_.flush(); // Ensure immediate write
    }
}
