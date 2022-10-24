#pragma once
#include <cstdint>

namespace MAIKo2Decoder
{
    using WordType = uint32_t;

    // const WordType EventHeader;
    // const WordType DataDelimiter; // between FADC data & TPC data
    // const WordType EventFooter;

    const WordType EventHeader = 0xeb901964;
    const WordType DataDelimiter = 0x160317ff; // between FADC data & TPC data
    const WordType EventFooter = 0x75504943;
}