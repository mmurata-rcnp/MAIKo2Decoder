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