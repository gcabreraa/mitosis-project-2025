import paho.mqtt.client as mqtt
import base64
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
from meshtastic.protobuf import mqtt_pb2, mesh_pb2
from meshtastic import protocols

import html
from markupsafe import escape
import re

BROKER = "localhost"
USER = ""
PASS = ""
PORT = 1883
TOPICS = ["test/#"]
KEY = "AQ=="
KEY = "1PG7OiApB1nwvP+rz05pAQ==" if KEY == "AQ==" else KEY


import sqlite3 # for database
from datetime import datetime, timedelta

MQTT_DB = "mqtt_stuff.db" #example database storing mqtt publishings
conn = sqlite3.connect(MQTT_DB)  
c = conn.cursor()  
c.execute('''CREATE TABLE IF NOT EXISTS mqtt_table (log_timestamp timestamp, data text);''')
conn.commit()
conn.close()

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
    
    if len(payload) != 10: # each measurement string should be 10 long without checksum
       return {"WRONG_LENGTH": payload, "LEN": len(payload)}

    # decoding functions for values in measurements
    def decode_char_pair(chars, offset):
        int_val = ord(chars[0]) - offset
        dec_val = int(chars[1])
        return int_val + dec_val / 10.0

    def decode_lux(chars):
        high = ord(chars[0]) - 32
        low = ord(chars[1]) - 32
        return high * 100 + low
    
    if len(payload) == 10: # confirm payload is correct length just in case
        
            temp = decode_char_pair(payload[0:2], temp_offset)
            hum  = decode_char_pair(payload[2:4], 32)
            lux  = decode_lux(payload[4:6])
            soc  = decode_char_pair(payload[6:8], 32)

            timestamp = server_receive_time - timedelta(seconds=(interval_seconds) * (12 - 1 - idx)) # compute real timestamp

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
            
def decode_multiple_payloads(pnum, concatenated_payload, server_receive_time): # decode string with multiple measurements

    if (len(concatenated_payload) != 132): # should be 12 concatenated strings of len 11
        return []
    
    decoded_results = []
    chunk_size = 11  # 10 data + 1 checksum
    num_chunks = 12
    
    print(concatenated_payload)

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

# Callback when the client connects to the broker
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker!")
        for topic in TOPICS:
            client.subscribe(topic)
            print(f"Subscribed to topic: {topic}")
    else:
        print(f"Failed to connect, return code {rc}")

# Callback when a message is received
def on_message(client, userdata, msg):
    print("ONE")
    se = mqtt_pb2.ServiceEnvelope()
    print("TWO")
    try:
        se.ParseFromString(msg.payload)
        decoded_mp = se.packet
    except:
        print(msg.payload)
        return
    print("THREE")
    # Try to decrypt the payload if it is encrypted
    if decoded_mp.HasField("encrypted") and not decoded_mp.HasField("decoded"):
        decoded_data = decrypt_packet(decoded_mp, KEY)
        if decoded_data is None:
            print("Decryption failed; retaining original encrypted payload")
        else:
            decoded_mp.decoded.CopyFrom(decoded_data)

    # Attempt to process the decrypted or encrypted payload
    portNumInt = decoded_mp.decoded.portnum if decoded_mp.HasField("decoded") else None
    handler = protocols.get(portNumInt) if portNumInt else None
    print("FOUR")
    pb = None
    if handler is not None and handler.protobufFactory is not None:
        pb = handler.protobufFactory()
        pb.ParseFromString(decoded_mp.decoded.payload)
    print("FIVE")
    if pb:
        # Clean and update the payload
        pb_str = str(pb).replace('\n', ' ').replace('\r', ' ').strip()
        decoded_mp.decoded.payload = pb_str.encode("utf-8")
    print("SIX")
    print(decoded_mp)
    conn = sqlite3.connect(MQTT_DB)
    c = conn.cursor()
    now = datetime.now()
    stuff = html.escape(str(decoded_mp))
    c.execute("""INSERT into mqtt_table VALUES (?,?);""",(now,f"{stuff}"))
    conn.commit()
    conn.close()
    
    conn = sqlite3.connect(MQTT_SERVER)
    c = conn.cursor()
    stuff = html.escape(str(decoded_mp))
    
    log_time = datetime.now()
    new_payload = extract_clean_payload(f"{stuff}")
    pmatch = extract_portnum(f"{stuff}")
    decoded_list = decode_multiple_payloads(pmatch, new_payload, log_time)
    
    for decoded in decoded_list:
            
        if isinstance(decoded, dict):
            c.execute(
                '''INSERT INTO rht_table (sensor_id, loc, rh, t, bat, lux, timing) VALUES (?, ?, ?, ?, ?,?, ?);''',
                (decoded["sensor_id"], decoded["loc"], decoded["humidity"], decoded["temperature"], decoded["soc"], decoded["lux"], decoded["timestamp"])
            )
        else:
            print(f"Skipping non-dict decoded: {decoded}")
            

    conn.commit()
    conn.close()


def decrypt_packet(mp, key):
    try:
        key_bytes = base64.b64decode(key.encode('ascii'))

        # Build the nonce from message ID and sender
        nonce_packet_id = getattr(mp, "id").to_bytes(8, "little")
        nonce_from_node = getattr(mp, "from").to_bytes(8, "little")
        nonce = nonce_packet_id + nonce_from_node

        # Decrypt the encrypted payload
        cipher = Cipher(algorithms.AES(key_bytes), modes.CTR(nonce), backend=default_backend())
        decryptor = cipher.decryptor()
        decrypted_bytes = decryptor.update(getattr(mp, "encrypted")) + decryptor.finalize()

        # Parse the decrypted bytes into a Data object
        data = mesh_pb2.Data()
        data.ParseFromString(decrypted_bytes)
        return data

    except Exception as e:
        return None

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.username_pw_set(USER, PASS)
try:
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_forever()
except Exception as e:
    print(f"An error occurred: {e}")
