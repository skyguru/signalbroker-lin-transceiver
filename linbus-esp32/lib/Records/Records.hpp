#pragma once

#include <cstdint>
#include <vector>

#include "Record.hpp"

class Records
{
public:
    Records(int size);

    Record *getRecord(int index);
    Record *getRecordById(int id);

    void add(Record record);
    int recordEntries() { return _recordEntries; }

private:
    std::vector<Record> _recordList;
    int _recordEntries;
};