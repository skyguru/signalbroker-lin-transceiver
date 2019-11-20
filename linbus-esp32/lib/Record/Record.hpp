
#include <cstdint>
#include <array>
#include <Arduino.h>

class Record
{

public:
    Record();

public:
    uint8_t id() const { return _id; }
    uint8_t size() const { return _size; }
    uint8_t master() const { return _master; }
    bool cacheValid() const { return _cacheValid; }
    uint8_t getWriteCacheByteByIndex(int index);
    std::array<uint8_t, 8> *writeCache();

    void setId(uint8_t id);
    void setSize(uint8_t size);
    void setMaster(uint8_t master);
    void setCacheValid(bool cacheValid);
    void setWriteCache(std::array<uint8_t, 8> *writeCache);

private:
    uint8_t _id;
    uint8_t _size;
    uint8_t _master;

    std::array<uint8_t, 8> _writeChache{};
    bool _cacheValid;
};