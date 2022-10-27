#pragma once
#include <cstdint>
#include <string>

namespace MAIKo2Decoder
{
    struct RawEventsRecord
    {
        uint32_t run_id;
        uint32_t plane_id;
        uint32_t board_id;
        uint32_t file_number;
        uint64_t event_id;
        uint64_t event_data_address;
        uint32_t event_data_length;
        uint32_t event_fadc_words_offset;
        uint32_t event_tpc_words_offset;
        uint32_t event_clock_counter;
        uint32_t event_trigger_counter;
    };

    struct RawFilesRecord
    {
        uint32_t run_id;
        uint32_t plane_id;
        uint32_t board_id;
        uint32_t file_number;
        std::string file_path; // varchar(100)
        inline static const unsigned int LengthLimitOfFilePath = 100;
    };

    struct PlanesRecord
    {
        uint32_t plane_id;
        std::string plane_name; // varchar(20)
        inline static const unsigned int LengthLimitOfPlaneName = 20;
    };
}