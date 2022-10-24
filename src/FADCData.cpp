#include "FADCData.hpp"

namespace MAIKo2Decoder
{
    FADCData::FADCData(const std::vector<WordType> &_words)
        : fGood(false), fEmpty(false), fSignals(), fErrorLog()
    {
        if (_words.size() == 0)
        {
            fEmpty = true;
        }
        else if (_words.size() % 2 == 0)
        {
            for (auto it = _words.begin(); it != _words.end(); it = it + 2)
            {
                auto word1 = *it;
                auto word2 = *(it + 1);

                ShortWordType sWord0 = (word1 & 0xffff0000) >> 16;
                ShortWordType sWord1 = (word1 & 0x0000ffff);
                ShortWordType sWord2 = (word2 & 0xffff0000) >> 16;
                ShortWordType sWord3 = (word2 & 0x0000ffff);

                if (CheckFormat(sWord0) && GetChannel(sWord0) == 0 &&
                    CheckFormat(sWord1) && GetChannel(sWord1) == 1 &&
                    CheckFormat(sWord2) && GetChannel(sWord2) == 2 &&
                    CheckFormat(sWord3) && GetChannel(sWord3) == 3)
                {
                    fSignals.at(0).push_back(GetSignal(sWord0));
                    fSignals.at(1).push_back(GetSignal(sWord1));
                    fSignals.at(2).push_back(GetSignal(sWord2));
                    fSignals.at(3).push_back(GetSignal(sWord3));
                }
                else
                {
                    fErrorLog << "Format Error in " << (it - _words.begin()) / 2 << " th clock " << std::endl;
                    fErrorLog << "    0: " << std::hex << sWord0 << ", "
                              << "Format " << (0xc000 & sWord0) << " (== 0x4000?), " << std::dec
                              << "Channel " << GetChannel(sWord0) << " (== 0?), "
                              << "Signal " << GetSignal(sWord0) << std::endl;
                    fErrorLog << "    1: " << std::hex << sWord1 << ", "
                              << "Format " << (0xc000 & sWord1) << " (== 0x4000?), " << std::dec
                              << "Channel " << GetChannel(sWord1) << " (== 1?), "
                              << "Signal " << GetSignal(sWord1) << std::endl;
                    fErrorLog << "    2: " << std::hex << sWord1 << ", "
                              << "Format " << (0xc000 & sWord2) << " (== 0x4000?), " << std::dec
                              << "Channel " << GetChannel(sWord2) << " (== 2?), "
                              << "Signal " << GetSignal(sWord2) << std::endl;
                    fErrorLog << "    3: " << std::hex << sWord3 << ", "
                              << "Format " << (0xc000 & sWord3) << " (== 0x4000?), " << std::dec
                              << "Channel " << GetChannel(sWord3) << " (== 3?), "
                              << "Signal " << GetSignal(sWord3) << std::endl;
                }
            }

            // Check validity
            // 2 words == 1 clock
            int signalLengthExpected = _words.size() / 2;
            int signalLengthAccepted = fSignals.at(0).size();
            if (signalLengthExpected == signalLengthAccepted)
            {
                fGood = true;
            }
        }
    }
}
