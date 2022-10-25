#pragma once
#include <vector>
#include <stdexcept>

#include "DecoderFormat.hpp"

namespace MAIKo2Decoder
{

    class EventWordsBuffer
    {
    public:
        EventWordsBuffer()
            : fValid(false), fWords(0),
              fEventFADCWordsOffset(0), fEventTPCWordsOffset(0) {}

        EventWordsBuffer(const std::vector<WordType> &_fWords)
            : fValid(false), fWords(_fWords),
              fEventFADCWordsOffset(0), fEventTPCWordsOffset(0)
        {
            auto result = EventValidation(fWords);
            if (result.fUnexpectedError)
                throw std::runtime_error("Unexpected error occurred in EventWordsBuffer::EventValidation(). Bug?");
            if (result.fGoodEvent &&
                CheckIndex(fWords, result.fEventFADCWordsOffset, result.fEventTPCWordsOffset))
            {
                fValid = true;
                fEventFADCWordsOffset = result.fEventFADCWordsOffset;
                fEventTPCWordsOffset = result.fEventTPCWordsOffset;
            }
        }

        EventWordsBuffer(const std::vector<WordType> &_fWords,
                         unsigned int _fEventFADCWordsOffset,
                         unsigned int _fEventTPCWordsOffset)
            : fValid(false), fWords(_fWords),
              fEventFADCWordsOffset(_fEventFADCWordsOffset), fEventTPCWordsOffset(_fEventTPCWordsOffset)
        {
            if (CheckIndex(fWords, _fEventFADCWordsOffset, _fEventTPCWordsOffset))
            {
                fValid = true;
            }
        }

        bool IsValid() const { return fValid; }

        std::vector<WordType> GetWords() const { return fWords; };
        unsigned int GetEventFADCWordsOffset() const { return fEventFADCWordsOffset; }
        unsigned int GetEventTPCWordsOffset() const { return fEventTPCWordsOffset; }

        std::vector<WordType> GetCounterWords() const;
        std::vector<WordType> GetFADCWords() const;
        std::vector<WordType> GetTPCWords() const;

        struct ValidationResult
        {
            bool fGoodEvent;
            bool fMultipleDelimiters;
            bool fUnexpectedError;
            unsigned int fEventFADCWordsOffset;
            unsigned int fEventTPCWordsOffset;
        };

        // Validate if the event buffer obeys the data format
        // If valid, make word index (FADC & TPC word offsets)
        static ValidationResult EventValidation(const std::vector<WordType> &_fWords);

        // Check if the event buffer can be divided into CounterData, FADCData, and TPCData.
        static bool CheckIndex(const std::vector<WordType> &_fWords,
                               unsigned int _fEventFADCWordOffset, unsigned int _fEventTPCWordOffset);

    private:
        // Validity of words buffer. True only if the event buffer can be divided into CounterData, FADCData, and TPCData.
        bool fValid;
        std::vector<WordType> fWords;
        unsigned int fEventFADCWordsOffset;
        unsigned int fEventTPCWordsOffset;
    };
}