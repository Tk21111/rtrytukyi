#include "logger.h"
#include <filesystem>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    try {
        // Create log directory if it doesn't exist
        std::filesystem::create_directories("log");
        
        logFile.open("log/debug.log", std::ios::app);
        if (!logFile.is_open()) {
            std::cerr << "Failed to open log file!" << std::endl;
        }
        
        WriteLog("INFO", "Logger initialized");
    }
    catch (const std::exception& e) {
        std::cerr << "Logger initialization error: " << e.what() << std::endl;
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        WriteLog("INFO", "Logger shutdown");
        logFile.close();
    }
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Logger::WriteLog(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    
    std::string logMessage = "[" + GetTimestamp() + "] [" + level + "] " + message;
    
    // Write to console
    std::cout << logMessage << std::endl;
    
    // Write to file
    if (logFile.is_open()) {
        logFile << logMessage << std::endl;
        logFile.flush();
    }
}

void Logger::Log(const std::string& message) {
    WriteLog("LOG", message);
}

void Logger::Error(const std::string& message) {
    WriteLog("ERROR", message);
}

void Logger::Info(const std::string& message) {
    WriteLog("INFO", message);
}