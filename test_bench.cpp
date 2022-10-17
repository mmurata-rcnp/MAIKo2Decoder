#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <array>
#include <sstream>
#include <functional>

uint32_t CorrectRawWord(const uint32_t &_wordIn)
{
    return ((_wordIn & 0xff000000) >> 24) |
           ((_wordIn & 0x00ff0000) >> 8) |
           ((_wordIn & 0x0000ff00) << 8) |
           ((_wordIn & 0x000000ff) << 24);
}

uint32_t ReadNextWord(std::ifstream &_fIn)
{
    uint32_t wordTmp;
    _fIn.read((char *)&wordTmp, sizeof(uint32_t)); // read next word
    return CorrectRawWord(wordTmp);
}

std::vector<uint32_t> ReadNextWords(std::ifstream &_fIn, unsigned int _nWords)
{
    std::vector<uint32_t> ret(_nWords, 0x00000000);
    std::for_each(ret.begin(), ret.end(),
                  [&_fIn](auto &_val)
                  { _val = ReadNextWord(_fIn); });
    return ret;
}

class CounterData
{
public:
    CounterData(const std::vector<uint32_t> &_words)
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

    bool IsGood() const { return fGood; };
    uint32_t GetTriggerCounter() const { return fTriggerCounter; };
    uint32_t GetClockCounter() const { return fClockCounter; };
    uint32_t GetCounter2() const { return fCounter2; };

private:
    bool fGood;
    uint32_t fTriggerCounter;
    uint32_t fClockCounter;
    uint32_t fCounter2;
};

class FADCData
{
public:
    FADCData(std::vector<uint32_t> &_words)
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

                uint16_t sWord0 = (word1 & 0xffff0000) >> 16;
                uint16_t sWord1 = (word1 & 0x0000ffff);
                uint16_t sWord2 = (word2 & 0xffff0000) >> 16;
                uint16_t sWord3 = (word2 & 0x0000ffff);

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

    bool IsGood() const { return fGood; };
    bool IsEmpty() const { return fEmpty; };
    std::vector<uint16_t> GetSignal(int _ch) const
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
    std::array<std::vector<uint16_t>, 4> fSignals;
    std::ostringstream fErrorLog;
    static bool CheckFormat(const uint16_t &_sWord) { return (_sWord & 0xc000) == 0x4000; }
    static int GetChannel(const uint16_t &_sWord) { return (_sWord & 0x3000) >> 12; }
    static uint16_t GetSignal(const uint16_t &_sWord) { return (_sWord & 0x03ff); }
};

class TPCData
{
public:
    TPCData(std::vector<uint32_t> &_words)
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
                uint32_t headerWord = *it;
                std::array<uint32_t, 4> words;
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
                        uint32_t stripShift = iWord * nBitsWord;
                        auto word = words.at(iWord);
                        for (unsigned int iBit = 0; iBit < nBitsWord; ++iBit)
                        {
                            const uint32_t stripInner = iBit;
                            // Position of the bit. Count from the most significant bit.
                            const uint32_t bitShift = iBit;
                            if (word & (0x80000000 >> (bitShift)))
                            {
                                fTPCHits.push_back(Hit(stripInner + stripShift, clock));
                            }
                        }
                    }
                }
                else
                {
                    fErrorLog << "Format Error in " << (it - _words.begin()) / 5 << " th clock " << std::endl;
                    fErrorLog << "    Header " << std::hex << headerWord << ", "
                              << "Format " << (0xffff0000 & headerWord) << " (== 0x80000000?), " << std::dec << std::endl;
                    fErrorLog << "    word1 " << std::dec << words.at(0) << std::dec << std::endl;
                    fErrorLog << "    word2 " << std::dec << words.at(1) << std::dec << std::endl;
                    fErrorLog << "    word3 " << std::dec << words.at(2) << std::dec << std::endl;
                    fErrorLog << "    word4 " << std::dec << words.at(3) << std::dec << std::endl;
                }
            }
        }
        // Check Validity (No error log?)
        if (fErrorLog.str() == "")
        {
            fGood = true;
        }
    }

    bool IsGood() const { return fGood; };
    bool IsEmpty() const { return fEmpty; };

    struct Hit
    {
        Hit(uint32_t _strip,
            uint32_t _clock) : strip(_strip), clock(_clock){};
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
    static bool CheckHeaderFormat(const uint32_t &_word) { return (_word & 0xffff0000) == 0x80000000; }
    static uint32_t GetClock(const uint32_t &_word) { return (_word & 0x0000ffff); };
};

template <typename T>
std::string MakeAA(const std::vector<T> &_vals,
                   const std::function<double(T, unsigned int, unsigned int)> _funcX,
                   const std::function<double(T, unsigned int, unsigned int)> _funcY,
                   unsigned int _xDiv, double _xMin, double _xMax,
                   unsigned int _yDiv, double _yMin, double _yMax)
{
    const unsigned int nVals = _vals.size();
    std::vector<std::pair<double, double>> xys;
    for (unsigned int i = 0; i < nVals; ++i)
    {
        auto x = _funcX(_vals.at(i), i, nVals);
        auto y = _funcY(_vals.at(i), i, nVals);
        xys.push_back(std::make_pair(x, y));
    }

    if (_xMin >= _xMax || _yMin >= _yMax)
        return "";

    std::vector<std::vector<unsigned int>> histogram(_yDiv, std::vector<unsigned int>(_xDiv, 0));
    // Fill histogram
    std::for_each(xys.begin(), xys.end(),
                  [&](auto _xy)
                  {
                      auto x = _xy.first;
                      auto y = _xy.second;
                      if (_xMin <= x && x < _xMax &&
                          _yMin <= y && y < _yMax)
                      {
                          unsigned int iBinX = (x - _xMin) / (double)(_xMax - _xMin) * _xDiv;
                          unsigned int iBinY = (y - _yMin) / (double)(_yMax - _yMin) * _yDiv;
                          //   std::cout << iBinX << " " << iBinY << std::endl;
                          histogram.at(iBinX).at(iBinY) += 1;
                      }
                  });

    std::ostringstream tmp;

    for (int iY = _yDiv - 1; iY >= 0; --iY)
    {
        for (int iX = 0; iX < (int)_xDiv; ++iX)
        {
            std::cout << iX << " " << iY << std::endl;
            tmp << ((histogram.at(iX).at(iY) > 0) ? 'X' : ' ');
        }
        tmp << std::endl;
    }
    return tmp.str();
}

int main()
{

    std::ifstream fIn("../../../data/uTPC_0040_cathode1_00000.raw", std::ios::binary);

    if (!fIn.good())
    {
        std::cerr << "No file" << std::endl;
        return 1;
    }
    // uint32_t wordTmp;

    // for (auto i = 0; i < 5000; ++i)
    // {
    //     fIn.read((char *)&wordTmp, sizeof(wordTmp));
    //     uint32_t word = Convert(wordTmp);
    //     // std::cout << std::hex << word << " " << Convert(word) << std::endl;
    //     std::cout << std::hex << std::setw(8) << std::setfill('0') << word << std::endl;
    //     switch (word)
    //     {
    //     case (0xeb901964):
    //         std::cout << "    EVENT HEADER" << std::endl;
    //         break;
    //     case (0x160317ff):
    //         std::cout << "    FADC FOOTER" << std::endl;
    //         break;
    //     case (0x75504943):
    //         std::cout << "    EVENT FOOTER" << std::endl;
    //         break;
    //     default:
    //         break;
    //     }
    // }

    const uint32_t kEventHeader = 0xeb901964;
    const uint32_t kDataDelimiter = 0x160317ff; // between FADC data & TPC data
    const uint32_t kEventFooter = 0x75504943;

    // Seek 1st event
    while (true)
    {
        if (!fIn.good())
        {
            std::cerr << "Error before reaching 1 st event" << std::endl;
            return 1;
        }
        // fIn.read((char *)&wordTmp, sizeof(uint32_t)); // read next word
        uint32_t word = ReadNextWord(fIn);
        if (word == kEventHeader) // 1 st event is found
        {
            fIn.seekg(-sizeof(uint32_t), std::ios_base::cur); // back to the position of the header
            break;
        }
    }

    std::cout << fIn.tellg() << std::endl;

    // fIn.read((char *)&wordTmp, sizeof(uint32_t)); // read next word
    // uint32_t word = Convert(wordTmp);
    // std::cout << std::hex << std::setw(8) << std::setfill('0') << word << std::endl;

    uint64_t count = 0;
    while (true)
    {
        if (!fIn.good())
        {
            std::cerr << "Error occurred while searching next event" << std::endl;
            return 1;
        }

        auto posHeader = fIn.tellg();
        uint32_t wordHeader = ReadNextWord(fIn);

        if (fIn.eof())
        {
            break;
        }

        if (wordHeader != kEventHeader)
        {
            std::cerr << "Format error: Event header is expected. -> " << std::hex << wordHeader << std::endl;
            return 1;
        }

        auto wordTriggerCounter = ReadNextWord(fIn);
        auto wordClockCounter = ReadNextWord(fIn);
        auto wordCounter2 = ReadNextWord(fIn);

        // Find event footer
        while (true)
        {
            auto word = ReadNextWord(fIn);
            if (word == kEventFooter)
            {
                // strict check : next word must be the header or EOF
                auto wordNext = ReadNextWord(fIn);
                if (wordNext == kEventHeader)
                {
                    fIn.seekg(-sizeof(uint32_t), std::ios_base::cur); // back to the position of the header
                    // fIn.seekg(posTmp, std::ios_base::beg);
                    break;
                }
                else if (fIn.eof())
                {
                    // std::cout << std::hex << wordNext << std::dec << std::endl;
                    fIn.clear(); // Release lock on the stream to keep good bit of the stream on.
                    break;
                }
                else
                {
                    continue;
                }
            }
            else if (fIn.eof())
            {
                std::cerr << "Format error: No event footer till the end of the file." << std::endl;
                return 1;
            }
        }

        auto posFooter = fIn.tellg();
        std::cout << wordTriggerCounter << " " << wordClockCounter << " : " << posHeader << " -- " << posFooter - posHeader << std::endl;
        count++;

        // std::cout << "    pos before read " << fIn.tellg() << std::endl;
        fIn.seekg(posHeader, std::ios_base::beg);
        // std::cout << "    pos after read " << fIn.tellg() << std::endl;
        auto wordsEvent = ReadNextWords(fIn, (posFooter - posHeader) / sizeof(uint32_t));
        auto nDelim = std::count(wordsEvent.begin(), wordsEvent.end(), kDataDelimiter);

        if (nDelim == 0)
            std::cout << "    NO TPC data" << std::endl;
        else if (nDelim == 1)
            std::cout << "    TPC data found" << std::endl;
        else
        {
            std::cerr << "    Multiple TPC data candidates found" << std::endl;
            std::cerr << "        Use first one." << std::endl;
        }

        // 2050
        std::cout << "    delim pos " << std::find(wordsEvent.begin(), wordsEvent.end(), kDataDelimiter) - wordsEvent.begin() << std::endl;

        auto posDelim = std::find(wordsEvent.begin(), wordsEvent.end(), kDataDelimiter);
        std::vector<uint32_t> wordsCounter;
        std::copy(wordsEvent.begin() + 1, wordsEvent.begin() + 4, std::back_inserter(wordsCounter));

        std::vector<uint32_t> wordsFADC;
        if (nDelim > 0)
            std::copy(wordsEvent.begin() + 4, posDelim, std::back_inserter(wordsFADC));

        std::vector<uint32_t> wordsTPC;
        if (nDelim > 0)
            std::copy(posDelim + 1, wordsEvent.end() - 1, std::back_inserter(wordsTPC));

        // std::cout << "    " << wordsCounter[0] << " " << std::hex << wordsFADC[0] << " " << wordsTPC[0] << std::dec << std::endl;
        std::cout << "    " << wordsCounter.size() << " " << wordsFADC.size() << " " << wordsTPC.size() << std::endl;

        CounterData counter(wordsCounter);

        FADCData fadc(wordsFADC);
        // std::cout << fadc.GetErrorLog() << std::endl;

        // auto signal0 = fadc.GetSignal(2);
        // std::for_each(signal0.begin(), signal0.end(), [](auto _val)
        //               { std::cout << _val << " "; });

        TPCData tpc(wordsTPC);
        std::cout << tpc.GetErrorLog() << std::endl;
        auto hits = tpc.GetHits();
        std::for_each(hits.begin(), hits.end(),
                      [](auto _hit)
                      { std::cout << "[" << _hit.strip << ", " << _hit.clock << "] "; });

        auto aa = MakeAA(
            hits,
            std::function<double(TPCData::Hit, unsigned int, unsigned int)>([](TPCData::Hit _hit, unsigned int, unsigned int) -> double
                                                                            { return (double)_hit.strip; }),
            std::function<double(TPCData::Hit, unsigned int, unsigned int)>([](TPCData::Hit _hit, unsigned int, unsigned int) -> double
                                                                            { return (double)_hit.clock; }),
            // [](TPCData::Hit _hit, unsigned int, unsigned int) -> double
            // { return (double)_hit.clock; },
            64u, 0., 128.,
            64u, 0., 1024.);

        std::cout << aa << std::endl;

        // DEBUG
        break;
    }

    std::cout << "Event count " << count << std::endl;

    return 0;
}