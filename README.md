# MAIKo2 decoder

## Overview
This is a decoder tool for the raw data acquired with the MAIKo+ active target.\
By using "make_index", you can add records to the raw data index tables, which is required by MAIKo2_JSON_API.


## Requirement
- CMake

- PostgreSQL
    - Role and database with the same name as your username

- C++ Libraries 
    - pqxx
    - nlohmann/json

## Preparation
- Create tables in DB
    - 3 tables
        - Raw files table: for the names of raw data files
        - Raw events table: for the positions of each events in the raw data file
        - Planes table: for the names of detector planes (not used so far)

    - This is an example of SQL in the case you name the tables as belows
        - Raw files table: `test.raw_files`
        - Raw events table: `test.raw_events`
        - Planes table: `test.planes`
    ``` create_index_tables.sql  
    CREATE TABLE IF NOT EXISTS test.raw_events (
        run_id integer NOT NULL,
        plane_id integer NOT NULL,
        board_id integer NOT NULL,
        file_number integer NOT NULL,
        event_id bigint NOT NULL,
        event_data_address bigint NOT NULL, 
        event_data_length integer NOT NULL,
        event_fadc_words_offset integer NOT NULL,
        event_tpc_words_offset integer NOT NULL,
        event_clock_counter bigint NOT NULL,
        event_trigger_counter bigint NOT NULL,
        PRIMARY KEY (run_id, plane_id, board_id, file_number, event_id)
    );

    CREATE TABLE IF NOT EXISTS test.planes (
        plane_id integer NOT NULL,
        plane_name varchar(20) NOT NULL,
        PRIMARY KEY (plane_id)
    );

    CREATE TABLE IF NOT EXISTS test.raw_files(
        run_id integer NOT NULL,
        plane_id integer NOT NULL,
        board_id integer NOT NULL,
        file_number integer NOT NULL,
        file_path varchar(100) NOT NULL,
        PRIMARY KEY (run_id, plane_id, board_id, file_number)
    );

    INSERT INTO test.planes (plane_id, plane_name) VALUES (0, 'anode');
    INSERT INTO test.planes (plane_id, plane_name) VALUES (1, 'cathode');
    ```

- Prepare config file
    - Must be named named "make_index.json"
    - Must be placed in any one of the following relative path from the build directory: `., ./config/, or ../input`.
    - Must contain the fields below
        - dataDirectoryPath: Path to raw data directory
        - rawDataFileFormat: Format of the name of raw data file. \$[run_id], \$[plane_id], \$[board_id], and \$[file_number] in the format is replaced.
        - optionsForConnectionToDB: Argument passed to the constructor of pqxx::connection for establishing connection to DB.
        - nameOfRawEventsTable: Name of "raw events table"
        - nameOfPlanesTable: Name of "planes table"
        - nameOfRawFilesTable: Name of "raw files tale"

    - This is an example of config. file
    ```make_index.json
    {
        "Memo": "config file in cogito",
        "dataDirectoryPath": "/home/quser/exp/maiko/2022Aut/data",
        "rawDataFileFormat": "uTPC_$[run_id]_$[plane]$[board_id]_$[file_number].raw",
        "optionsForConnectionToDB": "",
        "nameOfRawEventsTable": "test.raw_events",
        "nameOfPlanesTable": "test.planes",
        "nameOfRawFilesTable": "test.raw_files"
    }
    ```
    

- Build the program
    ```
    [Assume you are in the source directory named MAIKo2Decoder]
    $ mkdir ../build
    $ cd ../build
    $ cmake ../MAIKo2Decoder
    $ make
    ```

## Usage
```
$ ./make_index [run_id]
