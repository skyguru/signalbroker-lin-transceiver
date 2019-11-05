
#include <cstdint>
#include <HardwareSerial.h>

class Logger
{
public:
    Logger();
    Logger(HardwareSerial &serial);

public:
    enum class LogLevel : uint8_t
    {
        DEBUG = 0,
        INFO,
        WARNING,
        ERROR
    };

public:
    void setLogLevel(LogLevel &logLevel);

public:
    template <typename T>
    void debug(T data)
    {
        if (_logLevel == LogLevel::DEBUG)
        {
            _serial->print("DEBUG\t");
            _serial->println(data);
        }
    };

    template <typename T>
    void info(T data)
    {
        if (_logLevel <= LogLevel::INFO)
        {
            _serial->print("INFO\t");
            _serial->println(data);
        }
    }

    template <typename T>
    void warning(T data)
    {
        if (_logLevel <= LogLevel::WARNING)
        {
            _serial->print("WARNING\t");
            _serial->println(data);
        }
    }

    template <typename T>
    void error(T data)
    {
        _serial->print("ERROR\t");
        _serial->println(data);
    }

private:
    HardwareSerial *_serial;
    LogLevel _logLevel;
};