#include "TPCData.hpp"
#include <array>

namespace MAIKo2Decoder
{
    TPCData::TPCData(const std::vector<WordType> &_words)
        : fGood(false), fEmpty(false)
    {
        if (_words.size() == 0)
        {
            fEmpty = true;
        }
        else if (_words.size() % 5 == 0)
        {
            for (auto it = _words.begin(); it != _words.end(); it = it + 5)
            {
                WordType headerWord = *it;
                std::array<WordType, 4> words;
                words.at(0) = *(it + 1); // strip 000 -- 031
                words.at(1) = *(it + 2); // strip 032 -- 063
                words.at(2) = *(it + 3); // strip 064 -- 095
                words.at(3) = *(it + 4); // strip 096 -- 127

                if (CheckHeaderFormat(headerWord))
                {
                    auto clock = GetClock(headerWord);

                    // 32 bit (8 is number of bits in char type variable)
                    const auto nBitsWord = sizeof(words.at(0)) * 8;
                    for (unsigned int iWord = 0; iWord < words.size(); ++iWord)
                    {
                        // shift 32 strips for each words
                        const unsigned int stripShift = iWord * nBitsWord;
                        auto word = words.at(iWord);
                        for (unsigned int iBit = 0; iBit < nBitsWord; ++iBit)
                        {
                            const unsigned int stripInner = iBit;
                            // Position of the bit. Count from the most significant bit.
                            const unsigned int bitShift = iBit;
                            if (word & (0x80000000 >> (bitShift)))
                            {
                                fTPCHits.push_back(Hit(stripInner + stripShift, clock));
                            }
                        }
                    }
                }
                else
                {
                    fErrorLog << "Format error in " << (it - _words.begin()) / 5 << " th clock " << std::endl;
                    fErrorLog << "    Header " << std::hex << headerWord << ", "
                              << "Format " << (0xffff0000 & headerWord) << " (== 0x80000000?), " << std::dec << std::endl;
                    fErrorLog << "    word1 " << std::dec << words.at(0) << std::dec << std::endl;
                    fErrorLog << "    word2 " << std::dec << words.at(1) << std::dec << std::endl;
                    fErrorLog << "    word3 " << std::dec << words.at(2) << std::dec << std::endl;
                    fErrorLog << "    word4 " << std::dec << words.at(3) << std::dec << std::endl;
                }
            }
        }
        else
        {
            fErrorLog << "Format error: The length of TPC data must be 5n, but that of this event is " << _words.size() << std::endl;
        }
        // Check Validity (No error log?)
        if (fErrorLog.str() == "")
        {
            fGood = true;
        }
        else
        {
            fGood = false;
        }
    }
}