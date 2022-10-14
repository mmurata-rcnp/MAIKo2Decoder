#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>

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

int main()
{

    std::ifstream fIn("../../../data/uTPC_0040_anode1_00000.raw", std::ios::binary);

    if (!fIn.good())
    {
        std::cerr << "No file" << std::endl;
        return 1;
    }
    uint32_t wordTmp;

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
            return 1;
        }

        if (nDelim != 1)
            continue;

        // 2050
        std::cout << "    delim pos " << std::find(wordsEvent.begin(), wordsEvent.end(), kDataDelimiter) - wordsEvent.begin() << std::endl;

        auto posDelim = std::find(wordsEvent.begin(), wordsEvent.end(), kDataDelimiter);
        std::vector<uint32_t> wordsCounter(wordsEvent.begin() + 1, wordsEvent.begin() + 4);
        std::vector<uint32_t> wordsFADC(wordsEvent.begin() + 4, posDelim);
        std::vector<uint32_t> wordsTPC(posDelim + 1, wordsEvent.end());
        std::cout << "    " << wordsCounter[0] << " " << std::hex << wordsFADC[0] << " " << wordsTPC[0] << std::dec << std::endl;
    }

    std::cout << count << std::endl;

    return 0;
}