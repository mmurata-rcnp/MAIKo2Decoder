#pragma once
#include <cstdint>
#include <vector>
#include <fstream>
#include <map>

#include "DecoderFormat.hpp"

namespace MAIKo2Decoder
{

    // Return given 32-bit word in raw data, which is originally encoded in big endian, in right byte order.
    WordType CorrectRawWord(const WordType &_wordIn);

    // - Proceed the file pointer in raw-data file for 32-bit (1 word).
    // - Return byte-order corrected word read.
    WordType ReadNextWord(std::ifstream &_fIn);

    // - Proceed the file pointer in raw-data file for 32-bit x _nWords (_nWord words).
    // - Return vector of byte-order corrected words read.
    std::vector<WordType> ReadNextWords(std::ifstream &_fIn, unsigned int _nWords);

    // Return zero-filled string with its width is _digits
    // eg) _num:10, _digits:4 -> return "0010"
    std::string MakeZeroFilledUnsignedInteger(unsigned int _num, unsigned int _digits);

    // Return name of raw-data file in the format specified.
    // _fmt is the format of filename
    //     $[run_id] in _fmt is replaced with _run_id in of _run_id_digits integer filled with '0'
    //     $[plane] is replaced with _planeList[_plane_id]
    //     $[board_id] in _fmt is replaced with _board_id in of _board_id_digits integer filled with '0'
    //     $[file_number] in _fmt is replaced with _file_number in of _file_number_digits integer filled with '0'
    std::string GenerateFileName(const std::string &_fmt,
                                 unsigned int _run_id, unsigned int _run_id_digits,
                                 unsigned int _plane_id, const std::map<unsigned int, std::string> &_planeList,
                                 unsigned int _board_id, unsigned int _board_id_digits,
                                 unsigned int _file_number, unsigned int _file_number_digits);

    // Dump all of characters written the file specified with _filePath
    std::string FileDump(std::string _filePath);
}