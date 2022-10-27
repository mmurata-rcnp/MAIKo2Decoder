#include "DecoderUtility.hpp"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <regex>

namespace MAIKo2Decoder
{
    // Return given 32-bit word in raw data, which is originally encoded in big endian, in right byte order.
    WordType CorrectRawWord(const WordType &_wordIn)
    {
        return ((_wordIn & 0xff000000) >> 24) |
               ((_wordIn & 0x00ff0000) >> 8) |
               ((_wordIn & 0x0000ff00) << 8) |
               ((_wordIn & 0x000000ff) << 24);
    }

    // - Proceed the file pointer in raw-data file for 32-bit (1 word).
    // - Return byte-order corrected word read.
    WordType ReadNextWord(std::ifstream &_fIn)
    {
        WordType wordTmp;
        _fIn.read((char *)&wordTmp, sizeof(WordType)); // read next word
        return CorrectRawWord(wordTmp);
    }

    // - Proceed the file pointer in raw-data file for 32-bit x _nWords (_nWord words).
    // - Return vector of byte-order corrected words read.
    std::vector<WordType> ReadNextWords(std::ifstream &_fIn, unsigned int _nWords)
    {
        std::vector<WordType> ret(_nWords, 0x00000000);
        std::for_each(ret.begin(), ret.end(),
                      [&_fIn](auto &_val)
                      { _val = ReadNextWord(_fIn); });
        return ret;
    }

    // Return zero-filled string with its width is _digits
    // eg) _num:10, _digits:4 -> return "0010"
    std::string MakeZeroFilledUnsignedInteger(unsigned int _num, unsigned int _digits)
    {
        std::ostringstream tmp;
        tmp << std::setw(_digits) << std::setfill('0') << _num;
        return tmp.str();
    }

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
                                 unsigned int _file_number, unsigned int _file_number_digits)
    {
        std::string result(_fmt);
        result = std::regex_replace(result, std::regex(R"(\$\[run_id\])"),
                                    MakeZeroFilledUnsignedInteger(_run_id, _run_id_digits));
        if (_planeList.find(_plane_id) != _planeList.end())
            result = std::regex_replace(result, std::regex(R"(\$\[plane\])"),
                                        _planeList.at(_plane_id));
        result = std::regex_replace(result, std::regex(R"(\$\[board_id\])"),
                                    MakeZeroFilledUnsignedInteger(_board_id, _board_id_digits));
        result = std::regex_replace(result, std::regex(R"(\$\[file_number\])"),
                                    MakeZeroFilledUnsignedInteger(_file_number, _file_number_digits));

        return result;
    }

    // Dump all of characters written the file specified with _filePath
    std::string FileDump(std::string _filePath)
    {
        std::string sRet;
        try
        {
            std::ifstream in(_filePath.c_str());
            in.seekg(0, std::ios::end);
            sRet.resize(in.tellg());
            in.seekg(0, std::ios::beg);
            in.read(&sRet[0], sRet.size());
            in.close();
        }
        catch (...)
        {
            std::cerr << "File Load Error" << std::endl;
            sRet = "";
        }

        return sRet;
    }
}