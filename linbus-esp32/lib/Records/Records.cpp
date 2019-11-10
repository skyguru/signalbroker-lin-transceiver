#include "Records.hpp"

Records::Records(int size)
    : _recordEntries{size}
{
    // TODO: I don't think we need to initialize the vector with empty objects, we just need to reserve the space for performence issue

    // Initialize the vector with empty objects
    for (int number = 0; number < size; number++)
    {
        _recordList.push_back(Record{});
    }
}

/**
 * @breif: Returns the record on given index
 * @param: index - index of the vector
 * */
Record *Records::getRecord(int index)
{
    Record *record = &_recordList.at(index);
    return record;
}

/**
 * @breif: Returns a pointer to a record, if the id is valid
 * @param: id - the id of the record you are searching for
 * */
Record *Records::getRecordById(int id)
{
    Record *foundRecord = nullptr;
    for (Record &record : _recordList)
    {
        if (record.id() == id)
        {
            foundRecord = &record;
            break;
        }
    }
    return foundRecord;
}

/**
 * @breif: Add a record to the vector
 * @params: record - Record you want to add
 * */
void Records::add(Record record)
{
    _recordList.push_back(record);
}