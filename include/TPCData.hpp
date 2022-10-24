#pragma once
#include <vector>
#include <sstream>
#include "DecoderFormat.hpp"

namespace MAIKo2Decoder
{
    class TPCData
    {
    public:
        TPCData() : fGood(false), fEmpty(true), fTPCHits(), fErrorLog(){};
        TPCData(const std::vector<WordType> &_words);

        bool IsGood() const { return fGood; };
        bool IsEmpty() const { return fEmpty; };

        struct Hit
        {
            Hit() : strip(0), clock(0){};
            Hit(uint32_t _strip, uint32_t _clock)
                : strip(_strip), clock(_clock){};
            uint32_t strip; // x
            uint32_t clock; // y
        };

        std::vector<Hit> GetHits() const { return fTPCHits; };

        std::string GetErrorLog() const { return fErrorLog.str(); };

    private:
        bool fGood;
        bool fEmpty;
        std::vector<Hit> fTPCHits;
        std::ostringstream fErrorLog;
        static bool CheckHeaderFormat(const WordType &_word) { return (_word & 0xffff0000) == 0x80000000; }
        static unsigned int GetClock(const WordType &_word) { return (_word & 0x0000ffff); };
    };
}