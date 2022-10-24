#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <array>
#include <sstream>
#include <functional>

// #include <libpq-fe.h>
#include <pqxx/pqxx>

#include "DecoderFormat.hpp"
#include "DecoderUtility.hpp"
#include "CounterData.hpp"
#include "FADCData.hpp"
#include "TPCData.hpp"

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
            // std::cout << iX << " " << iY << std::endl;
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

    // const uint32_t kEventHeader = 0xeb901964;
    // const uint32_t kDataDelimiter = 0x160317ff; // between FADC data & TPC data
    // const uint32_t kEventFooter = 0x75504943;

    // Seek 1st event
    while (true)
    {
        if (!fIn.good())
        {
            std::cerr << "Error before reaching 1 st event" << std::endl;
            return 1;
        }
        // fIn.read((char *)&wordTmp, sizeof(uint32_t)); // read next word
        MAIKo2Decoder::WordType word = MAIKo2Decoder::ReadNextWord(fIn);
        if (word == MAIKo2Decoder::EventHeader) // 1 st event is found
        {
            fIn.seekg(-sizeof(MAIKo2Decoder::WordType), std::ios_base::cur); // back to the position of the header
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
        MAIKo2Decoder::WordType wordHeader = MAIKo2Decoder::ReadNextWord(fIn);

        if (fIn.eof())
        {
            break;
        }

        if (wordHeader != MAIKo2Decoder::EventHeader)
        {
            std::cerr << "Format error: Event header is expected. -> " << std::hex << wordHeader << std::endl;
            return 1;
        }

        auto wordTriggerCounter = MAIKo2Decoder::ReadNextWord(fIn);
        auto wordClockCounter = MAIKo2Decoder::ReadNextWord(fIn);
        auto wordCounter2 = MAIKo2Decoder::ReadNextWord(fIn);

        // Find event footer
        while (true)
        {
            auto word = MAIKo2Decoder::ReadNextWord(fIn);
            if (word == MAIKo2Decoder::EventFooter)
            {
                // strict check : next word must be the header or EOF
                auto wordNext = MAIKo2Decoder::ReadNextWord(fIn);
                if (wordNext == MAIKo2Decoder::EventHeader)
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
        auto wordsEvent = MAIKo2Decoder::ReadNextWords(fIn, (posFooter - posHeader) / sizeof(MAIKo2Decoder::WordType));
        auto nDelim = std::count(wordsEvent.begin(), wordsEvent.end(), MAIKo2Decoder::DataDelimiter);

        if (nDelim == 0)
            std::cout << "    NO TPC data" << std::endl;
        else if (nDelim == 1)
            std::cout << "    TPC data were found" << std::endl;
        else
        {
            std::cerr << "    Multiple TPC data candidates were found" << std::endl;
            std::cerr << "        Use first one." << std::endl;
        }

        // 2050
        std::cout << "    delim pos " << std::find(wordsEvent.begin(), wordsEvent.end(), MAIKo2Decoder::DataDelimiter) - wordsEvent.begin() << std::endl;

        auto posDelim = std::find(wordsEvent.begin(), wordsEvent.end(), MAIKo2Decoder::DataDelimiter);
        std::vector<MAIKo2Decoder::WordType> wordsCounter;
        std::copy(wordsEvent.begin() + 1, wordsEvent.begin() + 4, std::back_inserter(wordsCounter));

        std::vector<MAIKo2Decoder::WordType> wordsFADC;
        if (nDelim > 0)
            std::copy(wordsEvent.begin() + 4, posDelim, std::back_inserter(wordsFADC));

        std::vector<MAIKo2Decoder::WordType> wordsTPC;
        if (nDelim > 0)
            std::copy(posDelim + 1, wordsEvent.end() - 1, std::back_inserter(wordsTPC));

        // std::cout << "    " << wordsCounter[0] << " " << std::hex << wordsFADC[0] << " " << wordsTPC[0] << std::dec << std::endl;
        std::cout << "    " << wordsCounter.size() << " " << wordsFADC.size() << " " << wordsTPC.size() << std::endl;

        MAIKo2Decoder::CounterData counter(wordsCounter);

        MAIKo2Decoder::FADCData fadc(wordsFADC);
        // std::cout << fadc.GetErrorLog() << std::endl;

        // auto signal0 = fadc.GetSignal(2);
        // std::for_each(signal0.begin(), signal0.end(), [](auto _val)
        //               { std::cout << _val << " "; });

        MAIKo2Decoder::TPCData tpc(wordsTPC);
        std::cout << tpc.GetErrorLog() << std::endl;
        auto hits = tpc.GetHits();
        // std::for_each(hits.begin(), hits.end(),
        //               [](auto _hit)
        //               { std::cout << "[" << _hit.strip << ", " << _hit.clock << "] "; });

        // auto aa = MakeAA(
        //     hits,
        //     std::function<double(TPCData::Hit, unsigned int, unsigned int)>([](TPCData::Hit _hit, unsigned int, unsigned int) -> double
        //                                                                     { return (double)_hit.strip; }),
        //     std::function<double(TPCData::Hit, unsigned int, unsigned int)>([](TPCData::Hit _hit, unsigned int, unsigned int) -> double
        //                                                                     { return (double)_hit.clock; }),
        //     // [](TPCData::Hit _hit, unsigned int, unsigned int) -> double
        //     // { return (double)_hit.clock; },
        //     64u, 0., 128.,
        //     64u, 0., 1024.);

        // std::cout << aa << std::endl;

        // DEBUG
        // break;
    }

    std::cout << "Event count " << count << std::endl;

    return 0;
}