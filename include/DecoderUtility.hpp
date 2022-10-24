#pragma once
#include <cstdint>
#include <vector>
#include <fstream>
#include "DecoderFormat.hpp"

namespace MAIKo2Decoder
{
    WordType CorrectRawWord(const WordType &_wordIn);

    WordType ReadNextWord(std::ifstream &_fIn);

    std::vector<WordType> ReadNextWords(std::ifstream &_fIn, unsigned int _nWords);
}