#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <future>
#include <functional>
#include <iomanip>

#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

#include "DecoderFormat.hpp"
#include "DecoderUtility.hpp"
#include "CounterData.hpp"
#include "FADCData.hpp"
#include "TPCData.hpp"
#include "EventWordsBuffer.hpp"
#include "StreamRawData.hpp"
#include "IndexTableFormat.hpp"

struct ResultsOfThread
{
    uint32_t run_id;
    uint32_t plane_id;
    uint32_t board_id;
    std::vector<MAIKo2Decoder::StreamRawDataResult> stream_results;
    std::vector<MAIKo2Decoder::RawEventsRecord> records;
    std::vector<MAIKo2Decoder::RawFilesRecord> files;
};

class Configuration
{
public:
    Configuration()
        : fGood(false)
    {
        std::vector<std::string> pathCandidates{"./make_index.json",
                                                "./config/make_index.json",
                                                "../input/make_index.json"};
        for (auto cand : pathCandidates)
        {
            auto result = ReadJsonFile(cand);
            fLog << "[" << cand << "]" << std::endl;
            fLog << result.Dump() << std::endl;
            if (result.good)
            {
                fGood = true;
                fFilePath = cand;
                break;
            }
        }
    };

    Configuration(const std::string &_filePath) : fGood(false)
    {
        auto result = ReadJsonFile(_filePath);

        fLog << "[" << _filePath << "]" << std::endl;
        fLog << result.Dump() << std::endl;

        if (result.good)
        {
            fGood = true;
            fFilePath = _filePath;
        }
    }

    bool IsGood() const { return fGood; }

    std::string KeyOfDataDirectoryPath() const { return "dataDirectoryPath"; };
    std::string KeyOfRawDataFileFormat() const { return "rawDataFileFormat"; };
    std::string KeyOfOptionsForConnectionToDB() const { return "optionsForConnectionToDB"; };
    std::string KeyOfNameOfRawEventsTable() const { return "nameOfRawEventsTable"; };
    std::string KeyOfNameOfPlanesTable() const { return "nameOfPlanesTable"; };
    std::string KeyOfNameOfRawFilesTable() const { return "nameOfRawFilesTable"; };

    std::string GetDataDirectoryPath() const { return fDataDirectoryPath; };
    std::string GetRawDataFileFormat() const { return fRawDataFileFormat; }
    std::string GetOptionsForConnectionToDB() const { return fOptionsForConnectionToDB; };
    std::string GetNameOfRawEventsTable() const { return fNameOfRawEventsTable; };
    std::string GetNameOfPlanesTable() const { return fNameOfPlanesTable; }
    std::string GetNameOfRawFilesTable() const { return fNameOfRawFilesTable; }

    std::string Dump() const
    {
        std::ostringstream tmp;
        tmp << "FilePath : " << fFilePath << std::endl;
        tmp << "Good : " << std::boolalpha << IsGood() << std::endl;
        tmp << KeyOfDataDirectoryPath() << " : " << GetDataDirectoryPath() << std::endl;
        tmp << KeyOfRawDataFileFormat() << " : " << GetRawDataFileFormat() << std::endl;
        tmp << KeyOfOptionsForConnectionToDB() << " : " << GetOptionsForConnectionToDB() << std::endl;

        tmp << KeyOfNameOfRawEventsTable() << " : " << GetNameOfRawEventsTable() << std::endl;
        tmp << KeyOfNameOfPlanesTable() << " : " << GetNameOfPlanesTable() << std::endl;
        tmp << KeyOfNameOfRawFilesTable() << " : " << GetNameOfRawFilesTable() << std::endl;

        return tmp.str();
    };

    struct ReadJsonResultType
    {
        ReadJsonResultType() : good(true), file_open_failure(false), json_parse_error(false){};
        bool good;
        bool file_open_failure;
        bool json_parse_error;
        std::vector<std::string> missing_keys;
        std::string Dump() const
        {
            std::ostringstream tmp;
            tmp << "Good : " << std::boolalpha << good << std::endl;
            if (!good)
            {
                tmp << "    File Open Failure : " << std::boolalpha << file_open_failure << std::endl;
                tmp << "    Json Parse Error  : " << std::boolalpha << json_parse_error << std::endl;

                if (missing_keys.size() > 0)
                {
                    tmp << "    Missing Keys      : [ ";
                    for (const auto &key : missing_keys)
                    {
                        tmp << key << " ";
                    }
                    tmp << "]" << std::endl;
                }
            }
            return tmp.str();
        };
    };

    std::string GetLog() const { return fLog.str(); };

private:
    bool fGood;
    std::string fFilePath;
    std::string fDataDirectoryPath;        // "../../../data";
    std::string fRawDataFileFormat;        // "uTPC_$[run_id]_$[plane]$[board_id]_$[file_number].raw";
    std::string fOptionsForConnectionToDB; // ""
    std::string fNameOfRawEventsTable;     // "test.raw_events"
    std::string fNameOfPlanesTable;        // "test.planes"
    std::string fNameOfRawFilesTable;      // "test.raw_files"
    std::ostringstream fLog;

    ReadJsonResultType ReadJsonFile(std::string _path)
    {
        std::ifstream f(_path);
        if (!f.good())
        {
            ReadJsonResultType result;
            result.good = false;
            result.file_open_failure = true;
            return result;
        }

        nlohmann::json data;
        try
        {
            data = nlohmann::json::parse(f);
        }
        catch (std::exception &e)
        {
            ReadJsonResultType result;
            result.good = false;
            result.json_parse_error = true;
            return result;
        }

        if (!data.contains(KeyOfDataDirectoryPath()) ||
            !data.contains(KeyOfRawDataFileFormat()) ||
            !data.contains(KeyOfOptionsForConnectionToDB()) ||
            !data.contains(KeyOfNameOfRawEventsTable()) ||
            !data.contains(KeyOfNameOfPlanesTable()) ||
            !data.contains(KeyOfNameOfRawFilesTable()))
        {
            ReadJsonResultType result;
            result.good = false;
            if (!data.contains(KeyOfDataDirectoryPath()))
                result.missing_keys.push_back(KeyOfDataDirectoryPath());

            if (!data.contains(KeyOfRawDataFileFormat()))
                result.missing_keys.push_back(KeyOfRawDataFileFormat());

            if (!data.contains(KeyOfOptionsForConnectionToDB()))
                result.missing_keys.push_back(KeyOfOptionsForConnectionToDB());

            if (!data.contains(KeyOfNameOfRawEventsTable()))
                result.missing_keys.push_back(KeyOfNameOfRawEventsTable());

            if (!data.contains(KeyOfNameOfPlanesTable()))
                result.missing_keys.push_back(KeyOfNameOfPlanesTable());

            if (!data.contains(KeyOfNameOfRawFilesTable()))
                result.missing_keys.push_back(KeyOfNameOfRawFilesTable());

            return result;
        }

        fDataDirectoryPath = data[KeyOfDataDirectoryPath()].get<std::string>();
        fRawDataFileFormat = data[KeyOfRawDataFileFormat()].get<std::string>();
        fOptionsForConnectionToDB = data[KeyOfOptionsForConnectionToDB()].get<std::string>();
        fNameOfRawEventsTable = data[KeyOfNameOfRawEventsTable()].get<std::string>();
        fNameOfPlanesTable = data[KeyOfNameOfPlanesTable()].get<std::string>();
        fNameOfRawFilesTable = data[KeyOfNameOfRawFilesTable()].get<std::string>();
        return ReadJsonResultType();
    };
};

int main(int argc, char *argv[])
{

    // config input

    if (argc < 2)
    {
        std::cerr << "[Usage] : " << argv[0] << " [run_id] " << std::endl;
        return 1;
    }

    unsigned int run_id = atoi(argv[1]);

    Configuration config;
    if (!config.IsGood())
    {
        std::cerr << "[Error] : Failed to access valid configuration file." << std::endl;
        std::cerr << config.GetLog() << std::endl;
        return 1;
    }
    std::cout << config.Dump() << std::endl;

    // return 0;

    const std::map<unsigned int, std::string> planeList = {{0, "anode"},
                                                           {1, "cathode"}};

    // Check if the first (file_numer == 0) raw-data files for each boards exist
    const std::string dataDirectoryPath = config.GetDataDirectoryPath();
    const std::string rawDataFileFormat = config.GetRawDataFileFormat();
    const unsigned int nPlane = 2;
    const unsigned int nBoard = 6;

    for (unsigned int iPlane = 0; iPlane < nPlane; ++iPlane)
    {
        for (unsigned int iBoard = 0; iBoard < nBoard; ++iBoard)
        {
            unsigned int file_number = 0;
            std::string fileName = MAIKo2Decoder::GenerateFileName(rawDataFileFormat,
                                                                   run_id, 4,
                                                                   iPlane, planeList,
                                                                   iBoard, 1,
                                                                   file_number, 5);

            std::string filePath = dataDirectoryPath + "/" + fileName;
            if (filePath.length() > MAIKo2Decoder::RawFilesRecord::LengthLimitOfFilePath)
            {
                std::cerr << "[Error] : Raw data file name " << filePath << " exceeds the limit length, "
                          << " which is " << MAIKo2Decoder::RawFilesRecord::LengthLimitOfFilePath << std::endl;
                return 1;
            }

            std::ifstream tmp(filePath);
            if (!tmp.good())
            {
                std::cerr << "[Error] : Raw data file at " << filePath << " can not be opened." << std::endl;
                return 1;
            }
        }
    }

    // Stream files in parallel
    const unsigned int nThread = nPlane * nBoard;
    std::vector<std::future<ResultsOfThread>> vResultsFuture(nThread);
    for (unsigned int iPlane = 0; iPlane < nPlane; ++iPlane)
    {
        for (unsigned int iBoard = 0; iBoard < nBoard; ++iBoard)
        {
            unsigned int index = iPlane * nBoard + iBoard;
            vResultsFuture[index] = std::async(
                std::launch::async,
                [=](unsigned int _run_id, unsigned int _iPlane, unsigned int _iBoard)
                    -> ResultsOfThread
                {
                    unsigned int file_number = 0;

                    ResultsOfThread resultsOfThread;
                    resultsOfThread.run_id = _run_id;
                    resultsOfThread.plane_id = _iPlane;
                    resultsOfThread.board_id = _iBoard;
                    while (true)
                    {
                        std::string fileName = MAIKo2Decoder::GenerateFileName(rawDataFileFormat,
                                                                               run_id, 4,
                                                                               iPlane, planeList,
                                                                               iBoard, 1,
                                                                               file_number, 5);

                        std::string filePath = dataDirectoryPath + "/" + fileName;

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
                            // if (evt.event_id > 10)
                            //     return false;

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

    // Wait for end of stream and get result
    std::vector<ResultsOfThread> vResults;
    std::for_each(vResultsFuture.begin(), vResultsFuture.end(),
                  [&](auto &_result)
                  { vResults.push_back(_result.get()); });

    // Check result
    std::for_each(vResults.begin(), vResults.end(),
                  [](ResultsOfThread &_resultsThread)
                  {
                      std::vector<std::string> fileNames;

                      std::for_each(_resultsThread.stream_results.begin(),
                                    _resultsThread.stream_results.end(),
                                    [&](MAIKo2Decoder::StreamRawDataResult &_resultStream)
                                    {
                                        fileNames.push_back(_resultStream.input.fileName);
                                        std::cout << _resultStream.input.fileName << std::endl;
                                        std::cout << "Good   : " << _resultStream.goodFlag << std::endl;
                                        std::cout << "Events : " << _resultStream.number_of_events_processed << std::endl;
                                    });
                  });

    // Connect to db
    pqxx::connection c(config.GetOptionsForConnectionToDB());
    std::cout << "Connected to " << c.dbname() << '\n';

    // Insert (or update) result to the raw_files table in DB
    for (auto &result : vResults)
    {
        pqxx::work tx{c};
        try
        {
            for (auto &file : result.files)
            {
                std::ostringstream query;
                query << "INSERT INTO " << config.GetNameOfRawFilesTable() << " ("
                      << "run_id, plane_id, board_id, file_number, "
                      << "file_path"
                      << ") "
                      << "VALUES ("
                      << file.run_id << ", " << file.plane_id << ", " << file.board_id << ", " << file.file_number << ", "
                      << "'" << file.file_path << "'"
                      << ") "
                      << "ON CONFLICT (run_id, plane_id, board_id, file_number) "
                      << "DO UPDATE "
                      << "SET "
                      << "file_path = "
                      << "'" << file.file_path << "'"
                      << ";"
                      << std::endl;
                // std::cout << query.str() << std::endl;
                pqxx::result res(tx.exec(query.str()));
            }

            tx.commit();
        }
        catch (const pqxx::sql_error &_e)
        {
            std::cerr << "[Error] : SQL exception occurred while inserting file records for "
                      << "run " << result.run_id << ", plane " << result.plane_id << ", board " << result.board_id << " "
                      << "into raw_files." << std::endl;
            std::cerr << _e.what() << std::endl;
        }
        catch (const pqxx::usage_error &_e)
        {
            std::cerr << "[Error] : Some libpqxx usage exception occurred while inserting file records for "
                      << "run " << result.run_id << ", plane " << result.plane_id << ", board " << result.board_id << " "
                      << "into raw_files." << std::endl;
            std::cerr << _e.what() << std::endl;
        }
        catch (const std::exception &_e)
        {
            std::cerr << "[Error] : Some exception occurred while inserting file records for "
                      << "run " << result.run_id << ", plane " << result.plane_id << ", board " << result.board_id << " "
                      << "into raw_files." << std::endl;
            std::cerr << _e.what() << std::endl;
        }
    }

    // Insert (or update) result to the raw_events table in DB
    for (auto &result : vResults)
    {
        pqxx::work tx{c};
        try
        {
            for (auto &rec : result.records)
            {
                std::ostringstream query;
                query << "INSERT INTO " << config.GetNameOfRawEventsTable() << " ("
                      << "run_id, plane_id, board_id, file_number, event_id, "
                      << "event_data_address, event_data_length, "
                      << "event_fadc_words_offset, event_tpc_words_offset, "
                      << "event_clock_counter, event_trigger_counter"
                      << ") "
                      << "VALUES ("
                      << rec.run_id << ", " << rec.plane_id << ", " << rec.board_id << ", " << rec.file_number << ", " << rec.event_id << ", "
                      << rec.event_data_address << ", " << rec.event_data_length << ", "
                      << rec.event_fadc_words_offset << ", " << rec.event_tpc_words_offset << ", "
                      << rec.event_clock_counter << ", " << rec.event_trigger_counter
                      << ") "
                      << "ON CONFLICT (run_id, plane_id, board_id, file_number, event_id) "
                      << "DO UPDATE "
                      << "SET "
                      << "event_data_address = " << rec.event_data_address << ", "
                      << "event_data_length = " << rec.event_data_length << ", "
                      << "event_fadc_words_offset = " << rec.event_fadc_words_offset << ", "
                      << "event_tpc_words_offset = " << rec.event_tpc_words_offset << ", "
                      << "event_clock_counter = " << rec.event_clock_counter << ", "
                      << "event_trigger_counter = " << rec.event_trigger_counter
                      << ";"
                      << std::endl;
                pqxx::result res(tx.exec(query.str()));
            }

            tx.commit();
        }
        catch (const pqxx::sql_error &_e)
        {
            std::cerr << "[Error] : SQL exception occurred while inserting event records for "
                      << "run " << result.run_id << ", plane " << result.plane_id << ", board " << result.board_id << " "
                      << "into raw_events." << std::endl;
            std::cerr << _e.what() << std::endl;
        }
        catch (const pqxx::usage_error &_e)
        {
            std::cerr << "[Error] : Some libpqxx usage exception occurred while inserting event records for "
                      << "run " << result.run_id << ", plane " << result.plane_id << ", board " << result.board_id << " "
                      << "into raw_events." << std::endl;
            std::cerr << _e.what() << std::endl;
        }
        catch (const std::exception &_e)
        {
            std::cerr << "[Error] : Some exception occurred while inserting event records for "
                      << "run " << result.run_id << ", plane " << result.plane_id << ", board " << result.board_id << " "
                      << "into raw_events." << std::endl;
            std::cerr << _e.what() << std::endl;
        }
    }

    return 0;
}