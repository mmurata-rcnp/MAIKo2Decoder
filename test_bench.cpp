#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <array>
#include <sstream>
#include <functional>
#include <future>
#include <regex>
#include <utility>

// #include <libpq-fe.h>
#include <pqxx/pqxx>

#include "DecoderFormat.hpp"
#include "DecoderUtility.hpp"
#include "CounterData.hpp"
#include "FADCData.hpp"
#include "TPCData.hpp"
#include "EventWordsBuffer.hpp"
#include "StreamRawData.hpp"
#include "IndexTableFormat.hpp"

template <typename T>
std::string MakeAA(const std::vector<T> &_vals,
                   const std::function<double(T, unsigned int, unsigned int)> _funcX,
                   const std::function<double(T, unsigned int, unsigned int)> _funcY,
                   unsigned int _xDiv, double _xMin, double _xMax,
                   unsigned int _yDiv, double _yMin, double _yMax,
                   std::string _title = "")
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

    std::vector<std::vector<unsigned int>> histogram(_xDiv, std::vector<unsigned int>(_yDiv, 0));
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

    uint32_t aaWidth = _xDiv + 2;
    std::ostringstream tmp;
    tmp << std::setw(aaWidth) << _title.substr(0, aaWidth) << std::endl;
    {
        std::ostringstream tmpX;
        tmpX << "X : [" << _xMin << " -- " << _xMax << "]";
        tmp << std::setw(aaWidth) << tmpX.str().substr(0, aaWidth) << std::endl;
    }
    {
        std::ostringstream tmpY;
        tmpY << "Y : [" << _yMin << " -- " << _yMax << "]";
        tmp << std::setw(aaWidth) << tmpY.str().substr(0, aaWidth) << std::endl;
    }
    for (auto i = 0; i < (int)_xDiv + 2; ++i)
    {
        tmp << "-";
    }
    tmp << std::endl;

    for (int iY = _yDiv - 1; iY >= 0; --iY)
    {
        tmp << "|";
        for (int iX = 0; iX < (int)_xDiv; ++iX)
        {
            // std::cout << iX << " " << iY << std::endl;
            try
            {
                tmp << ((histogram.at(iX).at(iY) > 0) ? 'X' : ' ');
            }
            catch (std::exception &e)
            {
                std::cerr << e.what() << std::endl;
            }
        }
        tmp << "|";
        tmp << std::endl;
    }

    for (auto i = 0; i < (int)_xDiv + 2; ++i)
    {
        tmp << "-";
    }
    tmp << std::endl;
    return tmp.str();
}

struct ResultsOfThread
{
    std::vector<MAIKo2Decoder::StreamRawDataResult> stream_results;
    std::vector<MAIKo2Decoder::RawEventsRecord> records;
    std::vector<MAIKo2Decoder::RawFilesRecord> files;
};

struct EventIndex
{
    uint32_t run_id;
    uint32_t plane_id;
    uint32_t board_id;
    std::string file_path;
    uint64_t event_data_address;
    uint32_t event_data_length;
    uint32_t event_fadc_words_offset;
    uint32_t event_tpc_words_offset;

    std::string Dump() const
    {
        std::ostringstream tmp;
        tmp << "run_id                  : " << run_id << std::endl;
        tmp << "plane_id                : " << plane_id << std::endl;
        tmp << "board_id                : " << board_id << std::endl;
        tmp << "file_path               : " << file_path << std::endl;
        tmp << "event_data_address      : " << event_data_address << std::endl;
        tmp << "event_data_length       : " << event_data_length << std::endl;
        tmp << "event_fadc_words_offset : " << event_fadc_words_offset << std::endl;
        tmp << "event_tpc_words_offset  : " << event_tpc_words_offset << std::endl;

        return tmp.str();
    }
};

struct FragmentedEventData
{
    uint32_t run_id;
    uint32_t plane_id;
    uint32_t board_id;
    uint32_t event_number;
    MAIKo2Decoder::CounterData counter;
    MAIKo2Decoder::FADCData fadc;
    MAIKo2Decoder::TPCData tpc;
};

class BuiltEventData
{
public:
    using Hit = MAIKo2Decoder::TPCData::Hit;
    using ShortWordType = MAIKo2Decoder::FADCData::ShortWordType;

    BuiltEventData()
        : fHitMapper([](uint32_t _plane_id, uint32_t _board_id, Hit _hit) -> Hit
                     { return {_hit.strip + _board_id * 128, _hit.clock}; }),
          fFADCChMapper([](uint32_t _plane_id, uint32_t _board_id, uint32_t _ch) -> uint32_t
                        { return _ch + _board_id * MAIKo2Decoder::FADCData::NumberOfChannels; }){};

    BuiltEventData(std::function<Hit(uint32_t, uint32_t, Hit)> _fHitMapper,
                   std::function<uint32_t(uint32_t, uint32_t, uint32_t)> _fFADCChMapper)
        : fHitMapper(_fHitMapper),
          fFADCChMapper(_fFADCChMapper){};

    struct AddFragmentResult
    {
        AddFragmentResult()
            : good(false), fragment_key_duplication(false), map_duplication(false){};
        bool good;
        bool fragment_key_duplication;
        bool map_duplication;
    };

    AddFragmentResult AddFragment(const FragmentedEventData &_frg)
    {
        AddFragmentResult result;
        // Check duplication
        // Duplication of fragment key
        if (fEventFragments.count({_frg.plane_id, _frg.board_id}) > 0)
        {
            result.good = false;
            result.fragment_key_duplication = true;
            return result;
        }
        for (uint32_t iCh = 0; iCh < MAIKo2Decoder::FADCData::NumberOfChannels; ++iCh)
        {
            uint32_t chMapped = fFADCChMapper(_frg.plane_id, _frg.board_id, iCh);
            // Duplication in inverted map is detected
            if (fFADCInvertedMap.count({_frg.plane_id, chMapped}) > 0)
            {
                result.good = false;
                result.map_duplication = true;
                return result;
            }
        }

        // No duplication detected -> Add
        for (uint32_t iCh = 0; iCh < MAIKo2Decoder::FADCData::NumberOfChannels; ++iCh)
        {
            uint32_t chMapped = fFADCChMapper(_frg.plane_id, _frg.board_id, iCh);
            KeyOfFragment keyFragment(_frg.plane_id, _frg.board_id);
            // Register into inv-Map
            auto fragment_locator = std::make_pair(keyFragment, iCh);
            fFADCInvertedMap.insert(std::make_pair(FullyQualifiedChannelForFADC(_frg.plane_id, chMapped), fragment_locator));
            // Register into Fragments
            fEventFragments.insert(std::make_pair(keyFragment, _frg));
        }
        result.good = true;
        return result;
    };

    std::vector<Hit> GetHits(uint32_t _plane_id) const
    {

        std::vector<Hit> ret;
        for (auto &frg : fEventFragments)
        {
            if (frg.first.plane_id != _plane_id)
                continue;

            for (auto hit : frg.second.tpc.GetHits())
            {
                ret.emplace_back(fHitMapper(_plane_id, frg.second.board_id, hit));

                // std::cout << "Plane " << _plane_id
                //           << " board " << frg.second.board_id
                //           << " hit " << hit.strip << " " << hit.clock << std::endl;
                // std::cout << "    -> "
                //           << fHitMapper(_plane_id, frg.second.board_id, hit).strip << " "
                //           << fHitMapper(_plane_id, frg.second.board_id, hit).clock
                //           << std::endl;
            }
        }
        return ret;
    }

    std::vector<ShortWordType> GetSignal(uint32_t _plane_id, uint32_t _ch) const
    {
        auto itMap = fFADCInvertedMap.find({_plane_id, _ch});
        if (itMap == fFADCInvertedMap.end())
            return {};

        auto keyOfFragment = itMap->second.first;
        uint32_t ch = itMap->second.second;

        return fEventFragments.at(keyOfFragment).fadc.GetSignal(ch);
    }

    std::vector<uint32_t> GetAvailableFADCCh(uint32_t _plane_id) const
    {
        std::vector<uint32_t> ret;
        std::for_each(fFADCInvertedMap.begin(), fFADCInvertedMap.end(),
                      [&](auto &_item)
                      {
                          if (_item.first.plane_id == _plane_id)
                              ret.push_back(_item.first.ch);
                      });
        std::sort(ret.begin(), ret.end(), std::less<uint32_t>());
        return ret;
    }

private:
    // Mapper function for TPC Hit (plane_id, board_id, Hit) -> Hit (mapped)
    std::function<Hit(uint32_t, uint32_t, Hit)> fHitMapper;

    // Mapper function for FADC Ch (plane_id, board_id, ch) -> ch (mapped)
    std::function<uint32_t(uint32_t, uint32_t, uint32_t)> fFADCChMapper;

    struct FullyQualifiedChannelForFADC
    {
        FullyQualifiedChannelForFADC(uint32_t _plane_id, uint32_t _ch)
            : plane_id(_plane_id), ch(_ch){};
        uint32_t plane_id;
        uint32_t ch;
        bool operator<(const FullyQualifiedChannelForFADC &_rhs) const
        {
            if (plane_id != _rhs.plane_id)
                return plane_id < _rhs.plane_id;
            else if (ch != _rhs.ch)
                return ch < _rhs.ch;
            else
                return false;
        }
    };

    struct KeyOfFragment
    {
        uint32_t plane_id;
        uint32_t board_id;
        KeyOfFragment(uint32_t _plane_id, uint32_t _board_id)
            : plane_id(_plane_id), board_id(_board_id){};
        bool operator<(const KeyOfFragment &_rhs) const
        {
            if (plane_id != _rhs.plane_id)
                return plane_id < _rhs.plane_id;
            else if (board_id != _rhs.board_id)
                return board_id < _rhs.board_id;
            else
                return false;
        }
    };

    // Inverted map for fetching FADC signal efficiently
    std::map<FullyQualifiedChannelForFADC, std::pair<KeyOfFragment, uint32_t>> fFADCInvertedMap;

    // Fragment store
    std::map<KeyOfFragment, FragmentedEventData> fEventFragments;

    // uint32_t a;
};

int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        std::cerr << "[Usage] : " << argv[0] << " [run_id] [event_number]" << std::endl;
        return 1;
    }

    uint32_t run_id = atoi(argv[1]);

    // Identifier of the event
    //    Same to trigger_counter for now.
    uint32_t event_number = atoi(argv[2]);

    // Connect to db
    pqxx::connection c("");
    std::cout << "Connected to " << c.dbname() << '\n';

    pqxx::work tx{c};
    std::vector<EventIndex> vIndexes;
    try
    {
        std::ostringstream query;
        query << "SELECT "
              << "e.run_id, e.plane_id, e.board_id, f.file_path, "
              << "e.event_data_address, e.event_data_length, "
              << "e.event_fadc_words_offset, e.event_tpc_words_offset "
              << "FROM test.raw_events AS e "
              << "INNER JOIN test.raw_files AS f ON "
              << "e.run_id = f.run_id AND "
              << "e.plane_id = f.plane_id AND "
              << "e.board_id = f.board_id AND "
              << "e.file_number = f.file_number "
              << "WHERE "
              << "e.run_id = " << run_id << " AND "
              << "e.event_trigger_counter = " << event_number << " "
              << "ORDER BY (e.plane_id, e.board_id)"
              << ";"
              << std::endl;

        pqxx::result res(tx.exec(query.str()));

        for (auto row : res)
        {
            for (auto i = 0; i < (int)row.size(); ++i)
            {
                std::cout << row[i] << " ";
            }
            std::cout << std::endl;

            EventIndex ind;
            ind.run_id = row[0].as<decltype(ind.run_id)>();
            ind.plane_id = row[1].as<decltype(ind.plane_id)>();
            ind.board_id = row[2].as<decltype(ind.board_id)>();
            ind.file_path = row[3].as<decltype(ind.file_path)>();
            ind.event_data_address = row[4].as<decltype(ind.event_data_address)>();
            ind.event_data_length = row[5].as<decltype(ind.event_data_length)>();
            ind.event_fadc_words_offset = row[6].as<decltype(ind.event_fadc_words_offset)>();
            ind.event_tpc_words_offset = row[7].as<decltype(ind.event_tpc_words_offset)>();
            vIndexes.push_back(ind);
        }

        tx.commit();
    }
    catch (const pqxx::sql_error &_e)
    {
        std::cerr << "[Error] : SQL exception occurred while selecting event records for "
                  << "run " << run_id << ", event number " << event_number << " "
                  << "from DB." << std::endl;
        std::cerr << _e.what() << std::endl;
    }
    catch (const pqxx::usage_error &_e)
    {
        std::cerr << "[Error] : Some libpqxx usage exception occurred while selecting event records for "
                  << "run " << run_id << ", event number " << event_number << " "
                  << "from DB." << std::endl;
        std::cerr << _e.what() << std::endl;
    }
    catch (const std::exception &_e)
    {
        std::cerr << "[Error] : Some exception occurred while selecting event records for "
                  << "run " << run_id << ", event number " << event_number << " "
                  << "from DB." << std::endl;
        std::cerr << _e.what() << std::endl;
    }

    for (auto &ind : vIndexes)
    {
        std::cout << ind.Dump() << std::endl;
    }

    if (vIndexes.size() == 0)
    {
        std::cerr << "[Error] : Run " << run_id << ", event " << event_number << " is NOT found." << std::endl;
        return 1;
    }

    // Decode
    std::vector<FragmentedEventData> vFragments;
    for (auto &ind : vIndexes)
    {
        std::ifstream fIn(ind.file_path, std::ios::binary);
        if (!fIn.good())
        {
            std::cerr << "[Error] : File " << ind.file_path << " is NOT found." << std::endl;
            return 1;
        }
        fIn.seekg(ind.event_data_address, std::ios_base::beg);
        auto wordsEvent = MAIKo2Decoder::ReadNextWords(fIn, ind.event_data_length / sizeof(MAIKo2Decoder::WordType));
        MAIKo2Decoder::EventWordsBuffer buf(wordsEvent, ind.event_fadc_words_offset, ind.event_tpc_words_offset);

        if (!buf.IsValid())
        {
            std::cerr << "[Error] : Word buffer at " << ind.event_data_address << " "
                      << "in the file " << ind.file_path << " "
                      << "is in the wrong format." << std::endl;
            return 1;
        }

        MAIKo2Decoder::CounterData counter(buf.GetCounterWords());
        MAIKo2Decoder::FADCData fadc(buf.GetFADCWords());
        MAIKo2Decoder::TPCData tpc(buf.GetTPCWords());

        FragmentedEventData frg;
        frg.run_id = ind.run_id;
        frg.plane_id = ind.plane_id;
        frg.board_id = ind.board_id;
        frg.event_number = event_number;
        frg.counter = counter;
        frg.fadc = fadc;
        frg.tpc = tpc;

        vFragments.push_back(frg);

        if (!counter.IsGood() ||
            !fadc.IsGood() ||
            !tpc.IsGood())
        {
            std::cerr << "[Error] : Decode for the event at " << ind.event_data_address << " "
                      << "in the file " << ind.file_path << " "
                      << "failed." << std::endl;

            std::cerr << counter.IsGood() << " "
                      << fadc.IsGood() << " "
                      << tpc.IsGood() << std::endl;

            return 1;
        }
    }

    BuiltEventData data;
    for (auto &frg : vFragments)
    {
        auto result = data.AddFragment(frg);
        // std::cout << frg.plane_id << " " << frg.board_id << std::endl;
        if (!result.good)
        {
            std::cerr << "[Error] : Build for the event fragment with the plane_id " << frg.plane_id << " "
                      << "and the board_id " << frg.board_id << " "
                      << "failed." << std::endl;
            std::cerr << result.fragment_key_duplication << " " << result.map_duplication << std::endl;
        }
    }

    auto hitsA = data.GetHits(0);
    auto hitsC = data.GetHits(1);

    using Hit = MAIKo2Decoder::TPCData::Hit;
    auto aaA = MakeAA(
        hitsA,
        std::function<double(Hit, unsigned int, unsigned int)>([](Hit _hit, unsigned int, unsigned int) -> double
                                                               { return (double)_hit.strip; }),
        std::function<double(Hit, unsigned int, unsigned int)>([](Hit _hit, unsigned int, unsigned int) -> double
                                                               { return (double)_hit.clock; }),
        // [](TPCData::Hit _hit, unsigned int, unsigned int) -> double
        // { return (double)_hit.clock; },
        64u, 0., 768,
        32u, 0., 2048.,
        "Anode");

    auto aaC = MakeAA(
        hitsC,
        std::function<double(Hit, unsigned int, unsigned int)>([](Hit _hit, unsigned int, unsigned int) -> double
                                                               { return (double)_hit.strip; }),
        std::function<double(Hit, unsigned int, unsigned int)>([](Hit _hit, unsigned int, unsigned int) -> double
                                                               { return (double)_hit.clock; }),
        // [](TPCData::Hit _hit, unsigned int, unsigned int) -> double
        // { return (double)_hit.clock; },
        64u, 0., 768,
        32u, 0., 2048.,
        "Cathode");

    if (hitsA.size() > 0)
    {

        std::cout
            << std::max_element(hitsA.begin(), hitsA.end(), [](Hit l, Hit r) -> bool
                                { return l.strip < r.strip; })
                   ->strip
            << std::endl;

        std::cout
            << std::min_element(hitsA.begin(), hitsA.end(), [](Hit l, Hit r) -> bool
                                { return l.strip < r.strip; })
                   ->strip
            << std::endl;
    }

    std::istringstream isA(aaA);
    std::istringstream isC(aaC);

    std::string lineA, lineC;
    while (std::getline(isA, lineA) && std::getline(isC, lineC))
    {
        std::cout << lineA << "    " << lineC << std::endl;
    }
    // std::cout << aaA << std::endl;

    // std::cout << "==========" << std::endl;
    // std::cout << aaC << std::endl;

#if 0
    pqxx::connection c;
    std::cout << "Connected to " << c.dbname() << '\n';
    pqxx::work tx{c};
    try
    {
        std::ostringstream query;
        query << "INSERT INTO test.raw_events ("
              << "run_id, plane_id, board_id, file_number, event_id, "
              << "event_data_address, event_data_length, "
              << "event_fadc_words_offset, event_tpc_words_offset, "
              << "event_clock_counter, event_trigger_counter"
              << ")"
              << "VALUES ("
              << 1 << ", " << 0 << "," << 0 << ", " << 0 << ", " << 1 << ", "
              << 100 << ", " << 1000 << ", "
              << 4 << ", " << 2051 << ", "
              << 0 << ", " << 1
              << ") "
              << "ON CONFLICT (run_id, plane_id, board_id, file_number, event_id) "
              << "DO UPDATE "
              << "SET "
              << "event_data_address = " << 10 << ", "
              << "event_data_length = " << 10 << ", "
              << "event_fadc_words_offset = " << 10 << ", "
              << "event_tpc_words_offset = " << 10 << ", "
              << "event_clock_counter = " << 10 << ", "
              << "event_trigger_counter = " << 10
              << ";"
              << std::endl;
        pqxx::result res(tx.exec(query.str()));
        tx.commit();
    }
    catch (const pqxx::sql_error &_e)
    {
        std::cerr << _e.what() << " <- SQL" << std::endl;
    }
    catch (const pqxx::usage_error &_e)
    {
        std::cerr << _e.what() << " <- Usage" << std::endl;
    }
    catch (const std::exception &_e)
    {
        std::cerr << _e.what() << std::endl;
    }

    return 0;

    if (argc < 2)
    {
        std::cerr << "[Usage] : " << argv[0] << " [run_id] " << std::endl;
        return 1;
    }

    unsigned int run_id = atoi(argv[1]);

    std::map<unsigned int, std::string> planeList;
    planeList[0] = "anode";
    planeList[1] = "cathode";

    // std::vector<std::string> fileNames;
    // unsigned int fileNumber = 10;
    // for (int iPlane = 0; iPlane < 2; ++iPlane)
    // {
    //     for (int iBoard = 0; iBoard < 6; ++iBoard)
    //     {
    //         fileNames.push_back(GenerateFileName("uTPC_$[run_id]_$[plane]$[board_id]_$[file_number].raw",
    //                                              run_id, 4,
    //                                              iPlane, planeList,
    //                                              iBoard, 1,
    //                                              fileNumber, 5));
    //     }
    // }

    // std::for_each(fileNames.begin(), fileNames.end(), [](auto name)
    //               { std::cout << name << std::endl; });

    // return 0;

    const std::string data_directory_path = "../../../data";
    const unsigned int nPlane = 2;
    const unsigned int nBoard = 6;
    const unsigned int nThread = nPlane * nBoard;
    std::vector<std::future<ResultsOfThread>> results(nThread);
    for (unsigned int iPlane = 0; iPlane < nPlane; ++iPlane)
    {
        for (unsigned int iBoard = 0; iBoard < nBoard; ++iBoard)
        {
            unsigned int index = iPlane * nBoard + iBoard;
            results[index] = std::async(
                std::launch::async,
                [=](unsigned int _run_id, unsigned int _iPlane, unsigned int _iBoard)
                    -> ResultsOfThread
                {
                    unsigned int file_number = 0;

                    ResultsOfThread resultsOfThread;
                    while (true)
                    {
                        std::string fileName = MAIKo2Decoder::GenerateFileName("uTPC_$[run_id]_$[plane]$[board_id]_$[file_number].raw",
                                                                               run_id, 4,
                                                                               iPlane, planeList,
                                                                               iBoard, 1,
                                                                               file_number, 5);

                        std::string filePath = data_directory_path;
                        filePath += "/";
                        filePath += fileName;

                        {
                            std::ifstream tmp(filePath);
                            if (!tmp.good())
                                break;
                        }

                        MAIKo2Decoder::StreamRawDataInput inp;
                        inp.fileName = filePath;
                        MAIKo2Decoder::RawFilesRecord rec_files;
                        rec_files.run_id = _run_id;
                        rec_files.plane_id = _iPlane;
                        rec_files.board_id = iBoard;
                        rec_files.file_number = file_number;
                        rec_files.file_path = filePath;

                        std::vector<MAIKo2Decoder::RawEventsRecord> recs;
                        // Record template
                        MAIKo2Decoder::RawEventsRecord rec_temp;
                        rec_temp.run_id = _run_id;
                        rec_temp.plane_id = _iPlane;
                        rec_temp.board_id = iBoard;
                        rec_temp.file_number = file_number;

                        std::function<bool(MAIKo2Decoder::RawEventData)> callBack = [&recs, rec_temp](const MAIKo2Decoder::RawEventData &evt)
                        {
                            MAIKo2Decoder::CounterData counter(evt.words.GetCounterWords());
                            MAIKo2Decoder::FADCData fadc(evt.words.GetFADCWords());
                            MAIKo2Decoder::TPCData tpc(evt.words.GetTPCWords());

                            // debug
                            if (evt.event_id > 10)
                                return false;

                            if (!counter.IsGood() ||
                                !fadc.IsGood() ||
                                !tpc.IsGood())
                            {
                                return false;
                            }

                            MAIKo2Decoder::RawEventsRecord rec = rec_temp;
                            rec.event_id = evt.event_id;
                            rec.event_data_address = evt.event_data_address;
                            rec.event_data_length = evt.event_data_length;
                            rec.event_fadc_words_offset = evt.words.GetEventFADCWordsOffset();
                            rec.event_tpc_words_offset = evt.words.GetEventTPCWordsOffset();
                            rec.event_clock_counter = counter.GetClockCounter();
                            rec.event_trigger_counter = counter.GetTriggerCounter();
                            recs.push_back(rec);
                            return true;
                        };

                        auto resultOfStream = StreamRawData(inp, callBack);

                        resultsOfThread.stream_results.push_back(resultOfStream);
                        std::copy(recs.begin(), recs.end(), std::back_inserter(resultsOfThread.records));
                        resultsOfThread.files.push_back(rec_files);
                        ++file_number;
                    }
                    return resultsOfThread;
                },
                run_id, iPlane, iBoard);
        }
    }

    // Wait for end
    std::vector<ResultsOfThread> vResultsOfThreads;
    std::for_each(results.begin(), results.end(), [&](auto &_result)
                  { vResultsOfThreads.push_back(_result.get()); });

    std::cout << vResultsOfThreads.size() << std::endl;
    std::cout << vResultsOfThreads.at(0).records.size() << std::endl;
    std::cout << vResultsOfThreads.at(0).stream_results.size() << std::endl;

    std::for_each(vResultsOfThreads.begin(), vResultsOfThreads.end(),
                  [](ResultsOfThread &_result)
                  {
                      std::for_each(_result.records.begin(), _result.records.end(),
                                    [](auto &_rec)
                                    {
                                        std::cout << _rec.run_id << " "
                                                  << _rec.plane_id << " "
                                                  << _rec.board_id << " "
                                                  << _rec.event_id << " "
                                                  << _rec.event_data_address << " "
                                                  << _rec.event_data_length << " "
                                                  << _rec.event_fadc_words_offset << " "
                                                  << _rec.event_tpc_words_offset << " "
                                                  << _rec.event_clock_counter << " "
                                                  << _rec.event_trigger_counter << std::endl;
                                    });
                  });

    // std::cout << result.goodFlag << " "
    //           << result.fileNotFound << " "
    //           << result.noEventFooter << " "
    //           << result.errorWhileSearchingEvent << " "
    //           << result.invalidHeader << " "
    //           << result.noEventFooter << " "
    //           << result.number_of_events_processed << std::endl;

    return 0;

    MAIKo2Decoder::StreamRawDataInput inp;
    inp.fileName = "../../../data/uTPC_0073_anode1_00000.raw";

    std::vector<MAIKo2Decoder::RawEventsRecord> recs;
    // Record template
    MAIKo2Decoder::RawEventsRecord rec_temp;
    rec_temp.run_id = 40;
    rec_temp.plane_id = 0;
    rec_temp.board_id = 0;
    std::function<bool(MAIKo2Decoder::RawEventData)> callBack = [&recs, rec_temp](const MAIKo2Decoder::RawEventData &evt)
    {
        MAIKo2Decoder::CounterData counter(evt.words.GetCounterWords());
        MAIKo2Decoder::FADCData fadc(evt.words.GetFADCWords());
        MAIKo2Decoder::TPCData tpc(evt.words.GetTPCWords());
        // std::cout << evt.event_id << " "
        //           << evt.event_data_address << " "
        //           << evt.event_data_length << " "
        //           << std::endl;

        // debug
        if (evt.event_id > 10)
            return false;

        if (!counter.IsGood() ||
            !fadc.IsGood() ||
            !tpc.IsGood())
        {
            // std::cerr << counter.IsGood() << " "
            //           << fadc.IsGood() << " "
            //           << tpc.IsGood() << " "
            //           << std::endl;
            return false;
        }

        MAIKo2Decoder::RawEventsRecord rec = rec_temp;
        rec.event_id = evt.event_id;
        rec.event_data_address = evt.event_data_address;
        rec.event_data_length = evt.event_data_length;
        rec.event_fadc_words_offset = evt.words.GetEventFADCWordsOffset();
        rec.event_tpc_words_offset = evt.words.GetEventTPCWordsOffset();
        rec.event_clock_counter = counter.GetClockCounter();
        rec.event_trigger_counter = counter.GetTriggerCounter();
        recs.push_back(rec);
        return true;
    };

    auto result = StreamRawData(inp, callBack);
    std::cout << result.goodFlag << " "
              << result.fileNotFound << " "
              << result.noEventFooter << " "
              << result.errorWhileSearchingEvent << " "
              << result.invalidHeader << " "
              << result.noEventFooter << " "
              << result.number_of_events_processed << std::endl;

    std::for_each(recs.begin(), recs.end(), [](auto &rec)
                  { std::cout << rec.run_id << " "
                              << rec.plane_id << " "
                              << rec.board_id << " "
                              << rec.event_id << " "
                              << rec.event_data_address << " "
                              << rec.event_data_length << " "
                              << rec.event_fadc_words_offset << " "
                              << rec.event_tpc_words_offset << " "
                              << rec.event_clock_counter << " "
                              << rec.event_trigger_counter << std::endl; });

    return 0;

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
        // auto wordCounter2 = MAIKo2Decoder::ReadNextWord(fIn);

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
#endif
}