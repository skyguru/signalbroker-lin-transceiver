#pragma once

#include <cstdint>
#include <vector>

#include "Record.hpp"

class Records
{
public:
    Records();

    Record *getRecord(int index);
    Record *getRecordById(int id);

    void add(Record record);
    Record *getLastElement();

private:
    std::vector<Record> _recordList;
};