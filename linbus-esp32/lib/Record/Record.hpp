
#include <cstdint>

class Record
{

public:
    Record();

public:
    uint8_t id() const { return _id; }
    uint8_t size() const { return _size; }
    uint8_t master() const { return _master; }
    uint8_t *writeCache() { return _write_cache; } ;
    bool cacheValid() const { return _cache_valid; }

    void setId(uint8_t id);
    void setSize(uint8_t size);
    void setMaster(uint8_t master);
    void setWriteCache(uint8_t *writeCache);
    void setCacheValid(bool cacheValid);

private:
    uint8_t _id;
    uint8_t _size;
    uint8_t _master;
    uint8_t _write_cache[8];
    bool _cache_valid;
};