#pragma once

#include <string>
#include <cstdint>
#include <functional>
#include "EventWordsBuffer.hpp"

namespace MAIKo2Decoder
{

    struct StreamRawDataInput
    {
        std::string fileName;
    };

    struct StreamRawDataResult
    {
        bool goodFlag = false;
        bool fileNotFound = false;
        bool noEventFound = false;
        bool errorWhileSearchingEvent = false;
        bool invalidHeader = false;
        bool noEventFooter = false;
        bool eventFormatError = false;
        bool abortedByCallBack = false;
        StreamRawDataInput input;
        uint64_t number_of_events_processed = 0;
    };

    struct RawEventData
    {
        uint64_t event_id;           // the order of the events in the file (begin from 1. Should be same to trigger counter)
        uint64_t event_data_address; // the address (byte) of the event in raw data file
        uint32_t event_data_length;  // the length of the word sequence in byte.
        // uint32_t event_fadc_words_offset; // the order of the word where FADC data begins. 0 if no FADC data in the event.
        // uint32_t event_tpc_words_offset;  // the order of the word where TPC data begins. 0 if no TPC data in the event.
        // uint32_t event_clock_counter;     // the value of clock counter in CounterData
        // uint32_t event_trigger_counter;   // the value of trigger counter in CounterData
        MAIKo2Decoder::EventWordsBuffer words;
        // MAIKo2Decoder::CounterData counter;
        // MAIKo2Decoder::FADCData fadc;
        // MAIKo2Decoder::TPCData tpc;
    };

    // Stream raw-data-file named _input.file_name from its beginning.
    // _callBack function is called after each event is processed.
    StreamRawDataResult StreamRawData(StreamRawDataInput _input,
                                      std::function<bool(const RawEventData &)> _callBack);
}