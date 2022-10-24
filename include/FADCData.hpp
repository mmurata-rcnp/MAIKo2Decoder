#pragma once

#include <vector>
#include <array>
#include <sstream>
#include "DecoderFormat.hpp"

namespace MAIKo2Decoder
{
    class FADCData
    {
    public:
        using ShortWordType = uint16_t;
        FADCData() : fGood(false), fEmpty(true), fSignals(), fErrorLog() {}
        FADCData(const std::vector<WordType> &_words);
        bool IsGood() const { return fGood; };
        bool IsEmpty() const { return fEmpty; };
        std::vector<ShortWordType> GetSignal(int _ch) const
        {
            if (_ch < 0 || _ch > 3)
                return {};
            else
                return fSignals.at(_ch);
        }

        std::string GetErrorLog() const { return fErrorLog.str(); };

    private:
        bool fGood;
        bool fEmpty;
        std::array<std::vector<ShortWordType>, 4> fSignals;
        std::ostringstream fErrorLog;
        static bool CheckFormat(const ShortWordType &_sWord) { return (_sWord & 0xc000) == 0x4000; }
        static int GetChannel(const ShortWordType &_sWord) { return (_sWord & 0x3000) >> 12; }
        static ShortWordType GetSignal(const ShortWordType &_sWord) { return (_sWord & 0x03ff); }
    };
}
