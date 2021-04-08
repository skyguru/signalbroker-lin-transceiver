#pragma once

#include <cstdint>
#include <vector>
#include <Arduino.h>

#include "Record.hpp"

class Records {
public:

    /**
     * Storing records in a vector
     */
    Records() {
        // Reserving some slots, to reduce the amount of times of reallocation
        _recordList.reserve(30);
    };

    /**
    * @brief Add a record to the vector
    * @param record - Record you want to add
    * */
    void add(Record record) {
        _recordList.push_back(record);
    }

    /**
     * @brief Get a record on a indicated index
     * @param index - Index that you want to access in the vector
     * @return The record on the index
     */
    Record &getRecordRef(int index) {
        return _recordList.at(index);
    }

    /**
     * @brief Get a record that match the indicated index
     * @param id - The id of the record you want to access
     * @return the record with the matching id if exists, otherwise return the invalid recrod
     */
    Record &getRecordRefById(int id) {
        for (auto &record : _recordList) {
            if (record.id() == id) {
                return record;
            }
        }

        // if the ID isn't found, return the invalid record
        return _recordList.back();
    }

    /**
     * @brief Get all records
     * @return the entire vector of records
     */
    std::vector<Record> getRecords() {
        return _recordList;
    }

private:
    std::vector<Record> _recordList;
};