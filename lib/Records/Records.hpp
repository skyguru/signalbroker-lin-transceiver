#pragma once

#include <cstdint>
#include <vector>
#include <Arduino.h>

#include "Record.hpp"

class Records {
public:
    Records();

    // Record *getRecord(int index);

    // Record *getRecordById(int id);

    void add(Record record);

    // Record *getLastElement();

    Record &getRecordRef(int index) {
        return _recordList.at(index);
    }

   Record &getRecordRefById(int id) {
       for (auto &record : _recordList) {
           if (record.id() == id) {
               return record;
           }
       }

       // if the ID isn't found, return the invalid record
       return _recordList.back();
   }

    Record &getLastElementByRef() {
        return _recordList.back();
    }

private:
    std::vector<Record> _recordList;
};