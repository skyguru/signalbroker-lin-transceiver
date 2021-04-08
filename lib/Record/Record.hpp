#pragma once

#include <cstdint>
#include <cstring>
#include <array>


class Record {

public:
    /**
     * A record defines a LIN-frame
     */
    constexpr Record()
            : id_{0},
              size_{0},
              master_{0},
              cacheValid_{false} {}

public:

    /**
     * @brief Get the LINFrame id
     * @return record id
     */
    constexpr uint8_t id() const { return id_; }

    /**
     * @brief Get the LINFrame size
     * @return record size
     */
    constexpr uint8_t size() const { return size_; }

    /**
     * @brief Get the LINFrame type (master frame or not)
     * @return type of record (master or arbitration frame)
     */
    constexpr uint8_t master() const { return master_; }

    /**
     * @brief Check if the record cache is valid or not
     * @return cache status
     */
    constexpr bool cacheValid() const { return cacheValid_; }

    constexpr uint8_t getWriteCacheByteByIndex(int index) const { return writeCache_.at(index); }

    /**
    * @brief Return the entire write cache-buffer
    * @return write cache buffer
    * */
    std::array<uint8_t, 8> &writeCache() {
        return writeCache_;
    }

    /**
    * @brief This defines the id of the frame in the record
    * @param id: Id of the signal frame
    * */
    void setId(uint8_t id) {
        id_ = id;
    };

    /**
     * @brief This defines the size of the frame in the record
    * @param size: Size of the signal frame
    * */
    void setSize(uint8_t size) {
        size_ = size;
    };

    /**
    * @brief This defines if the record either is a master or a slave frame
    * @param master: 0=slave, 1=master
    * */
    void setMaster(uint8_t master) {
        master_ = master;
    };

    /**
    * @brief Change state of cache valid
    * @param cacheValid: True/false
    * */
    void setCacheValid(bool cacheValid) {
        cacheValid_ = cacheValid;
    }

    /**
    * @brief Set the data of write cache buffer
    * @param writeCache: array of data
    * */
    void setWriteCache(std::array<uint8_t, 8> *writeCache) {
        memcpy(&writeCache_, writeCache, writeCache->size());
    }

private:
    std::array<uint8_t, 8> writeCache_{};

    // LINFrame-ID
    uint8_t id_;

    // LINFrame-Size
    uint8_t size_;

    // LINFrame If the frame is a master frame(1) or arbitration frame(0)
    uint8_t master_;

    // LINFrame flag for if the record cache is valid
    bool cacheValid_;
};