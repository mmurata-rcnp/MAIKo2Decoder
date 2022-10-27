#include "StreamRawData.hpp"
#include <fstream>
#include "DecoderUtility.hpp"
#include "DecoderFormat.hpp"

namespace MAIKo2Decoder
{

    StreamRawDataResult StreamRawData(StreamRawDataInput _input,
                                      std::function<bool(const RawEventData &)> _callBack)
    {
        StreamRawDataResult result;
        result.input = _input;
        std::ifstream fIn(_input.fileName, std::ios::binary);

        if (!fIn.good())
        {
            result.fileNotFound = true;
            return result;
        }

        // Seek header of 1st event
        while (true)
        {
            if (!fIn.good())
            {
                result.noEventFound = true;
                return result;
            }

            MAIKo2Decoder::WordType word = MAIKo2Decoder::ReadNextWord(fIn);
            if (word == MAIKo2Decoder::EventHeader) // 1 st event is found
            {
                fIn.seekg(-sizeof(MAIKo2Decoder::WordType), std::ios_base::cur); // back to the position of the header
                break;
            }
        }

        while (true)
        {
            RawEventData evt;
            if (!fIn.good())
            {
                result.errorWhileSearchingEvent = true;
                return result;
            }

            auto posHeader = fIn.tellg();
            // Expect events begin from the header
            MAIKo2Decoder::WordType wordHeader = MAIKo2Decoder::ReadNextWord(fIn);

            if (fIn.eof())
            {
                break;
            }
            else if (wordHeader != MAIKo2Decoder::EventHeader)
            {
                result.invalidHeader = true;
                return result;
            }

            // Find event footer
            while (true)
            {
                auto word = MAIKo2Decoder::ReadNextWord(fIn);
                if (word == MAIKo2Decoder::EventFooter)
                {
                    // strict check : next word must be the header or EOF
                    auto wordNext = MAIKo2Decoder::ReadNextWord(fIn);
                    if (wordNext == MAIKo2Decoder::EventHeader) // Events continue in the file
                    {
                        fIn.seekg(-sizeof(uint32_t), std::ios_base::cur); // back to the position of the header
                        break;
                    }
                    else if (fIn.eof()) // It is the last event
                    {
                        fIn.clear(); // Release lock on the stream to keep good bit of the stream on.
                        break;
                    }
                    else // NOT a event footer (TPC data ?) -> proceed with searching event footer
                    {
                        continue;
                    }
                }
                else if (fIn.eof())
                {
                    result.noEventFooter = true;
                    return result;
                }
            }

            auto posFooter = fIn.tellg();

            fIn.seekg(posHeader, std::ios_base::beg);
            auto wordsEvent = MAIKo2Decoder::ReadNextWords(fIn, (posFooter - posHeader) / sizeof(MAIKo2Decoder::WordType));
            ++result.number_of_events_processed;
            evt.event_id = result.number_of_events_processed;
            evt.event_data_address = posHeader;
            evt.event_data_length = posFooter - posHeader;
            evt.words = MAIKo2Decoder::EventWordsBuffer(wordsEvent);

            if (!evt.words.IsValid())
            {
                result.eventFormatError = true;
                return result;
            }
            auto doContinue = _callBack(evt);
            if (!doContinue)
            {
                result.abortedByCallBack = true;
                return result;
            }
        }
        result.goodFlag = true;
        return result;
    };
}