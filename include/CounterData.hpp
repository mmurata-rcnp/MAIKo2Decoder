#pragma once
#include <cstdint>
#include <vector>

#include "DecoderFormat.hpp"

namespace MAIKo2Decoder
{

    class CounterData
    {
    public:
        CounterData()
            : fGood(false), fTriggerCounter(0x00000000),
              fClockCounter(0x00000000), fCounter2(0x00000000){};
        CounterData(const std::vector<WordType> &_words);

        bool IsGood() const { return fGood; };
        WordType GetTriggerCounter() const { return fTriggerCounter; };
        WordType GetClockCounter() const { return fClockCounter; };
        WordType GetCounter2() const { return fCounter2; };

    private:
        bool fGood;
        WordType fTriggerCounter;
        WordType fClockCounter;
        WordType fCounter2;
    };

}