#include "Record.hpp"

#include <Arduino.h>

Record::Record() {}

/**
 * @brief This defines the id of the frame in the record
 * @param id: Id of the signal frame 
 * */
void Record::setId(uint8_t id)
{
    _id = id;
}

/**
 * @brief This defines the size of the frame in the record
 * @param size: Size of the signal frame
 * */
void Record::setSize(uint8_t size)
{
    _size = size;
}

/**
 * @brief This defines if the record either is a master or a slave frame
 * @param master: 0=slave, 1=master
 * */
void Record::setMaster(uint8_t master)
{
    _master = master;
}

/**
 * @brief Change state of cache valid
 * @param cacheValid: True/false
 * */
void Record::setCacheValid(bool cacheValid)
{
    _cacheValid = cacheValid;
}

/**
 * @brief Returns a byte of choosen index from the array
 * @param index: Index of the array you want to get
 * @return Byte of buffer
 * */
uint8_t Record::getWriteCacheByteByIndex(int index)
{
    return _writeChache.at(index);
}

/**
 * @brief Return the entire writechache-buffer
 * @return writecache buffer
 * */
std::array<uint8_t, 8> *Record::writeCache()
{
    return &_writeChache;
}

/**
 * @brief Set the data of writechache buffer
 * @param writeCache: array of data 
 * */
void Record::setWriteCache(std::array<uint8_t, 8> *writeCache)
{
    memcpy(&_writeChache, writeCache, writeCache->size());
}