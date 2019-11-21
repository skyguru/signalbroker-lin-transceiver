#include "Records.hpp"

Records::Records()
{
    // Reserving 20-objects
    _recordList.reserve(20);
}

/**
 * @brief Returns the record on given index
 * @param index - index of the vector
 * */
Record *Records::getRecord(int index)
{
    Record *record = &_recordList.at(index);
    return record;
}

/**
 * @brief Returns a pointer to a record, if the id is valid
 * @param id - the id of the record you are searching for
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
 * @brief Add a record to the vector
 * @param record - Record you want to add
 * */
void Records::add(Record record)
{
    _recordList.push_back(record);
}

/**
 * @brief Returning the last element from the array of Records
 * */
Record *Records::getLastElement()
{
    return &_recordList.back();
}