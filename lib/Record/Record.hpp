#pragma once
#include <cstdint>
#include <cstring>
#include <array>


class Record
{

public:
    constexpr Record()
        : m_ID{0},
          m_Size{0},
          m_Master{0},
          m_CacheValid{false} {}

public:
    constexpr uint8_t id() const { return m_ID; }

    constexpr uint8_t size() const { return m_Size; }

    constexpr uint8_t master() const { return m_Master; }

    constexpr bool cacheValid() const { return m_CacheValid; }

    constexpr uint8_t getWriteCacheByteByIndex(int index) const { return m_WriteCache.at(index); }

    /**
    * @brief Return the entire write cache-buffer
    * @return write cache buffer
    * */
    std::array<uint8_t, 8> &writeCache()
    {
        return m_WriteCache;
    }

    /**
    * @brief This defines the id of the frame in the record
    * @param id: Id of the signal frame
    * */
    void setId(uint8_t id)
    {
        m_ID = id;
    };

    /**
     * @brief This defines the size of the frame in the record
    * @param size: Size of the signal frame
    * */
    void setSize(uint8_t size)
    {
        m_Size = size;
    };

    /**
    * @brief This defines if the record either is a master or a slave frame
    * @param master: 0=slave, 1=master
    * */
    void setMaster(uint8_t master)
    {
        m_Master = master;
    };

    /**
    * @brief Change state of cache valid
    * @param cacheValid: True/false
    * */
    void setCacheValid(bool cacheValid)
    {
        m_CacheValid = cacheValid;
    }

    /**
    * @brief Set the data of write cache buffer
    * @param writeCache: array of data
    * */
    void setWriteCache(std::array<uint8_t, 8> *writeCache)
    {
        memcpy(&m_WriteCache, writeCache, writeCache->size());
    }

private:
    std::array<uint8_t, 8> m_WriteCache{};
    uint8_t m_ID;
    uint8_t m_Size;
    uint8_t m_Master;
    bool m_CacheValid;
};