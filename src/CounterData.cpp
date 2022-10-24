#include "CounterData.hpp"

namespace MAIKo2Decoder
{

    CounterData::CounterData(const std::vector<WordType> &_words)
        : fGood(false), fTriggerCounter(0), fClockCounter(0), fCounter2(0)
    {
        if (_words.size() == 3)
        {
            fGood = true;
            fTriggerCounter = _words.at(0);
            fClockCounter = _words.at(1);
            fCounter2 = _words.at(2);
        }
    }

}