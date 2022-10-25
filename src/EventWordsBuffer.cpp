#include "EventWordsBuffer.hpp"
#include <algorithm>

namespace MAIKo2Decoder
{

    std::vector<WordType> EventWordsBuffer::GetCounterWords() const
    {
        if (!IsValid())
            return {};
        std::vector<WordType> ret;
        std::copy(fWords.begin() + 1, fWords.begin() + LengthOfCounterWords + 1, std::back_inserter(ret));
        return ret;
    }

    std::vector<WordType> EventWordsBuffer::GetFADCWords() const
    {
        if (!IsValid())
            return {};
        else if (fEventFADCWordsOffset == 0 &&
                 fEventTPCWordsOffset == 0)
            return {};
        std::vector<WordType> ret;
        std::copy(fWords.begin() + fEventFADCWordsOffset,
                  fWords.begin() + fEventTPCWordsOffset - 1, std::back_inserter(ret));
        return ret;
    }

    std::vector<WordType> EventWordsBuffer::GetTPCWords() const
    {
        if (!IsValid())
            return {};
        else if (fEventFADCWordsOffset == 0 &&
                 fEventTPCWordsOffset == 0)
            return {};
        std::vector<WordType> ret;
        std::copy(fWords.begin() + fEventTPCWordsOffset,
                  fWords.end() - 1, std::back_inserter(ret));
        return ret;
    }

    EventWordsBuffer::ValidationResult EventWordsBuffer::EventValidation(const std::vector<WordType> &_fWords)
    {
        ValidationResult result;
        result.fGoodEvent = false;
        result.fMultipleDelimiters = false;
        result.fUnexpectedError = false;
        result.fEventFADCWordsOffset = 0;
        result.fEventTPCWordsOffset = 0;

        if (_fWords.size() < 2) // No header or no footer
        {
            result.fGoodEvent = false;
            return result;
        }
        else if (_fWords.front() != EventHeader ||
                 _fWords.back() != EventFooter) // Invalid header or invalid footer
        {
            result.fGoodEvent = false;
            return result;
        }
        else if (_fWords.size() == LengthOfCounterWords + 2) // Counter + header + footer == no TPC, FADC data
        {
            result.fGoodEvent = true;
            result.fEventFADCWordsOffset = 0;
            result.fEventTPCWordsOffset = 0;
            return result;
        }

        // The event must contain FADC and TPC data
        //     --> Check FADC and TPC data
        // Number of delimiters
        auto nDelim = std::count(_fWords.begin(), _fWords.end(), DataDelimiter);
        // Position od delimiters
        auto posDelim = std::find(_fWords.begin(), _fWords.end(), DataDelimiter);
        if (nDelim == 0) // No data delimiter -> invalid
        {
            result.fGoodEvent = false;
            return result;
        }
        else if (nDelim > 1) // Multiple delimiter. ->  Record but proceed. The first delimiter is adopted.
        {
            result.fMultipleDelimiters = true;
        }
        else // Unique
        {
            result.fMultipleDelimiters = false;
        }
        // Can not determine the position of delimiter even delimiter is found. (NEVER HAPPEN)
        if (posDelim == _fWords.end())
        {
            // NEVER CALLED
            result.fGoodEvent = false;
            result.fUnexpectedError = true;
            return result;
            // throw std::runtime_error("Unexpected error in algorithm: Never be called in ValidateEventWords()");
        }

        result.fEventFADCWordsOffset = 1 + LengthOfCounterWords;      // Event header + counter
        result.fEventTPCWordsOffset = posDelim - _fWords.begin() + 1; // The next word of delimiter
        // Offset overrun (NEVER HAPPEN)
        if (result.fEventFADCWordsOffset >= _fWords.size() ||
            result.fEventTPCWordsOffset >= _fWords.size())
        {
            // NEVER CALLED
            result.fGoodEvent = false;
            result.fUnexpectedError = true;
            return result;
            // throw std::runtime_error("Unexpected error in offset decision: Never be called in ValidateEventWords() ");
        }

        result.fGoodEvent = true;
        return result;
    }

    bool EventWordsBuffer::CheckIndex(const std::vector<WordType> &_fWords,
                                      unsigned int _fEventFADCWordOffset, unsigned int _fEventTPCWordOffset)
    {
        if (_fWords.size() == 1 + LengthOfCounterWords + 1 &&
            _fEventFADCWordOffset == 0 &&
            _fEventTPCWordOffset == 0) // Short event (header + counter + footer)
        {
            return true;
        }
        else if (_fEventFADCWordOffset == 1 + LengthOfCounterWords &&
                 _fEventFADCWordOffset < _fEventTPCWordOffset &&
                 _fEventTPCWordOffset < _fWords.size() &&
                 _fWords.at(_fEventTPCWordOffset - 1) == DataDelimiter) // full event
        {
            return true;
        }
        else
        {
            return false;
        }
    }
}