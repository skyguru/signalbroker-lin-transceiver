#include "Record.hpp"

Record::Record()
{
}

void Record::setId(uint8_t id)
{
    _id = id;
}

void Record::setSize(uint8_t size)
{
    _size = size;
}

void Record::setMaster(uint8_t master)
{
    _master = master;
}

void Record::setWriteCache(uint8_t writeCache[])
{
}

void Record::setCacheValid(bool cacheValid)
{
    _cache_valid = cacheValid;
}