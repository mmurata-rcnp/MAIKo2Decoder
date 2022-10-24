#include "DecoderUtility.hpp"
#include <algorithm>

namespace MAIKo2Decoder
{
    WordType CorrectRawWord(const WordType &_wordIn)
    {
        return ((_wordIn & 0xff000000) >> 24) |
               ((_wordIn & 0x00ff0000) >> 8) |
               ((_wordIn & 0x0000ff00) << 8) |
               ((_wordIn & 0x000000ff) << 24);
    }

    WordType ReadNextWord(std::ifstream &_fIn)
    {
        WordType wordTmp;
        _fIn.read((char *)&wordTmp, sizeof(WordType)); // read next word
        return CorrectRawWord(wordTmp);
    }

    std::vector<WordType> ReadNextWords(std::ifstream &_fIn, unsigned int _nWords)
    {
        std::vector<WordType> ret(_nWords, 0x00000000);
        std::for_each(ret.begin(), ret.end(),
                      [&_fIn](auto &_val)
                      { _val = ReadNextWord(_fIn); });
        return ret;
    }
}