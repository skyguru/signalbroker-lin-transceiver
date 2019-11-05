#include "Log.hpp"

Logger::Logger()
{
}

Logger::Logger(HardwareSerial &serial)
    : _serial{&serial},
      _logLevel{LogLevel::DEBUG}
{
}

void Logger::setLogLevel(LogLevel &logLevel)
{
    _logLevel = logLevel;
}