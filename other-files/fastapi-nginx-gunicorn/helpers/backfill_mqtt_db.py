import html
import json
import re

import math

import sqlite3
from datetime import datetime, timedelta

MQTT_DB = "mqtt_stuff.db"  # Source database
MQTT_SERVER = "mqtt_measurements.db"  # Destination database

# mapping mqtt ids to locations
id_to_loc = {2956773168: "Sailing Pavilion",
            2956776044: "Outside 36",
            1136117244: "Stata",
            1128180828: "Mass Ave",
            1128122576: "Z-Center",
            1136043824: "Demo"
            }

# repeated functions from mqtt_runner for decoding
def extract_clean_payload(raw_string): 
    # Unescape HTML entities (&quot; -> ")
    unescaped = html.unescape(raw_string)

    # Use regex to find the payload value in quotes
    match = re.search(r'payload:\s+"(.*?)"', unescaped)
    if not match:
        return None
    
    # Unescape any escaped characters (like \\r -> \r)
    clean_payload = bytes(match.group(1), "utf-8").decode("raw_unicode_escape")
    
    return clean_payload.strip()

def extract_portnum(raw_string): # extract portnum (id) to map to location string
    unescaped = html.unescape(raw_string)
    pmatch = re.search(r'from:\s+(\S+)', unescaped)
    if not pmatch:
        return -1
    
    clean_pnum = bytes(pmatch.group(1), "utf-8").decode("raw_unicode_escape")
    
    return int(clean_pnum.strip())

def decode_sensor_data_updated(pnum, payload, server_receive_time, idx, interval_seconds=300, temp_offset=60):
    
    if len(payload) != 10:
       return {"WRONG_LENGTH": payload, "LEN": len(payload)}

    def decode_char_pair(chars, offset):
        int_val = ord(chars[0]) - offset
        dec_val = int(chars[1])
        return int_val + dec_val / 10.0


    def decode_lux(chars):
        high = ord(chars[0]) - 32
        low = ord(chars[1]) - 32
        return high * 100 + low
    
    if len(payload) == 10:
        
            temp = decode_char_pair(payload[0:2], temp_offset)
            hum  = decode_char_pair(payload[2:4], 32)
            lux  = decode_lux(payload[4:6])
            soc  = decode_char_pair(payload[6:8], 32)

            timestamp = server_receive_time - timedelta(seconds=(interval_seconds) * (12 - 1 - idx))

            return {
                "temperature": temp,
                "humidity": hum,
                "lux": lux,
                "soc": soc,
                "sensor_id": pnum,
                "loc": id_to_loc[pnum],
                "timestamp": timestamp,
                "received": server_receive_time
            }
            
def decode_multiple_payloads(pnum, concatenated_payload, server_receive_time):

    if (len(concatenated_payload) != 132):
        return []
    
    decoded_results = []
    chunk_size = 11  # 10 data + 1 checksum
    num_chunks = 12

    for i in range(num_chunks):
        start_idx = i * chunk_size
        end_idx = start_idx + chunk_size
        full_chunk = concatenated_payload[start_idx:end_idx]
        
        # Remove checksum character (last char)
        clean_payload = full_chunk[:-1]
        
        # Decode, different interval and offset parameters for earlier prototypes and demo
        if id_to_loc[pnum] == "Stata":
            decoded = decode_sensor_data_updated(pnum, clean_payload, server_receive_time, i, interval_seconds=25, temp_offset=100)
        elif id_to_loc[pnum] == "Outside 36":
            decoded = decode_sensor_data_updated(pnum, clean_payload, server_receive_time, i, temp_offset=100)
        elif id_to_loc[pnum] == "Demo":
            decoded = decode_sensor_data_updated(pnum, clean_payload, server_receive_time, i, interval_seconds=25)
        else:
            decoded = decode_sensor_data_updated(pnum, clean_payload, server_receive_time, i)
            
        decoded_results.append(decoded)
        
    return decoded_results
    

def get_decoded_data_simple():
    conn = sqlite3.connect(MQTT_DB)
    c = conn.cursor()

    query = """
        SELECT log_timestamp, data
        FROM mqtt_table
        WHERE log_timestamp > ?
        ORDER BY log_timestamp ASC
    """

    timestamp_cutoff = "2025-05-06 20:30:00"
    raw_rows = c.execute(query, (timestamp_cutoff,)).fetchall()
    conn.close()

    conn_dest = sqlite3.connect(MQTT_SERVER)
    c_dest = conn_dest.cursor()

    # Ensure destination table exists
    c_dest.execute('''
        CREATE TABLE IF NOT EXISTS rht_table (
            sensor_id REAL,
            loc REAL,
            rh REAL, 
            t REAL,  
            bat REAL,
            lux REAL,
            timing TIMESTAMP,
            server_time TIMESTAMP
        );
    ''')

    print("Created/confirmed rht_table")

    for log_time_str, str_record in raw_rows:
        try:
            log_time = datetime.fromisoformat(log_time_str)
        except ValueError:
            continue

        try:
            new_payload = extract_clean_payload(str_record)
            pmatch = extract_portnum(str_record)
            decoded_list = decode_multiple_payloads(pmatch, new_payload, log_time)
            
        except Exception as e:
            print(f"Skipping bad record: {e}")
            continue

        for decoded in decoded_list:
            
            if not isinstance(decoded, dict):
                continue

            try:
                c_dest.execute(
                    '''INSERT INTO rht_table (sensor_id, loc, rh, t, bat, lux, timing, server_time) VALUES (?, ?, ?, ?, ?, ?, ?, ?)''',
                    (
                        decoded.get("sensor_id"),
                        decoded.get("loc"),
                        decoded.get("humidity"),
                        decoded.get("temperature"),
                        decoded.get("soc"),
                        decoded.get("lux"),
                        decoded.get("timestamp"),
                        decoded.get("received")
                    )
                )

            except Exception as e:
                print(f"Failed to insert row: {e}")

    conn_dest.commit()
    conn_dest.close()
    print("Finished inserting all records.")

    
get_decoded_data_simple()