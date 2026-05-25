#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

class Logger {
public:
    static Logger& Instance();
    
    void Log(const std::string& message);
    void Error(const std::string& message);
    void Info(const std::string& message);
    
private:
    Logger();
    ~Logger();
    
    void WriteLog(const std::string& level, const std::string& message);
    std::string GetTimestamp();
    
    std::ofstream logFile;
    std::mutex logMutex;
};

// Convenience macros
#define LOG_INFO(msg) Logger::Instance().Info(msg)
#define LOG_ERROR(msg) Logger::Instance().Error(msg)
#define LOG(msg) Logger::Instance().Log(msg)