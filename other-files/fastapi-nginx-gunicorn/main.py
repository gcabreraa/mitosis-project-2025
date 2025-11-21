from typing import Annotated
from fastapi import FastAPI, Query
from fastapi.responses import HTMLResponse
from fastapi.responses import JSONResponse
from fastapi import Request
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from fastapi.responses import FileResponse
from fastapi.responses import JSONResponse
from fastapi.responses import Response
templates = Jinja2Templates(directory="templates")

import pandas as pd
import plotly
import plotly.express as px

import html
import json
import re

import math

import sqlite3
from datetime import datetime, timedelta
from pydantic import BaseModel

app = FastAPI()

EX_DB = "mqtt_measurements.db"
LOG_DB = "integrated_log.db"
DEFAULT_START = "2025-05-07T14:00"
DEFAULT_END = "2025-05-09T10:00"
DEFAULT_LOC = "1136117244"

max_readings = 12 # num readings per payload

class ErrorLogRequest(BaseModel):
    error_type: str
    error_msg: str

@app.get("/") # view home page, directs viewers to plot page
def home(request: Request):
    return templates.TemplateResponse("plotly_base.html", {"request": request})

def compute_heat_index(rh, t):
    t = t * 9/5 + 32
    hi = (-42.379 
          + 2.04901523 * t 
          + 10.14333127 * rh 
          - 0.22475541 * t * rh 
          - 0.00683783 * t * t 
          - 0.05481717 * rh * rh 
          + 0.00122874 * t * t * rh 
          + 0.00085282 * t * rh * rh 
          - 0.00000199 * t * t * rh * rh)

    if rh < 13 and 80 <= t <= 112:
        adjustment = ((13 - rh) / 4) * math.sqrt((17 - abs(t - 95)) / 17)
        hi -= adjustment
    elif rh > 85 and 80 <= t <= 87:
        adjustment = ((rh - 85) / 10) * ((87 - t) / 5)
        hi += adjustment

    return hi

def get_data(start_time, end_time, location): # queries data from mqtt_measurements.db for plots
    conn = sqlite3.connect(EX_DB)
    query = """
        SELECT timing, t, rh, bat, lux 
        FROM rht_table 
        WHERE loc = ?
    """
    params = [location]
    if start_time and end_time:
        start_time_dt = datetime.strptime(start_time, "%Y-%m-%dT%H:%M")
        end_time_dt = datetime.strptime(end_time, "%Y-%m-%dT%H:%M")
        query += " AND timing BETWEEN ? AND ?"
        params.extend([start_time_dt, end_time_dt])

    query += " ORDER BY timing DESC;"
    
    df = pd.read_sql_query(query, conn, params=params)
    conn.close()

    df['soc'] = df['bat']
    df['timing'] = pd.to_datetime(df['timing'])
    df['heat_index'] = [compute_heat_index(df['rh'][i], df['t'][i]) for i in range(len(df['rh']))]
    df['fahrenheit_t'] = [32 + (9/5)*df['t'][i] for i in range(len(df['t']))]
    return df


@app.get("/show_plots") # this is the main plot page! go here to view data
def get_plots(request: Request, start_time: str = Query(None), end_time: str = Query(None), location: str = Query(DEFAULT_LOC)):   
    df = get_data(start_time, end_time, location)
    
    fig_ctemp = px.line(df, x='timing', y='t', title='Temperature vs Time')
    fig_ctemp.update_xaxes(title_text="Time")
    fig_ctemp.update_yaxes(title_text="Temperature (Degrees Celsius)")
    graphJSON_ctemp = json.dumps(fig_ctemp, cls=plotly.utils.PlotlyJSONEncoder)
    
    fig_ftemp = px.line(df, x='timing', y='fahrenheit_t', title='Temperature vs Time')
    fig_ftemp.update_xaxes(title_text="Time")
    fig_ftemp.update_yaxes(title_text="Temperature (Degrees Fahrenheit)")
    graphJSON_ftemp = json.dumps(fig_ftemp, cls=plotly.utils.PlotlyJSONEncoder)

    fig_rh = px.line(df, x='timing', y='rh', title='Relative Humidity vs Time')
    fig_rh.update_xaxes(title_text="Time")
    fig_rh.update_yaxes(title_text="Relative Humidity (%)")
    graphJSON_rh = json.dumps(fig_rh, cls=plotly.utils.PlotlyJSONEncoder)

    fig_soc = px.line(df, x='timing', y='soc', title='State of Charge vs Time')
    fig_soc.update_xaxes(title_text="Time")
    fig_soc.update_yaxes(title_text="SOC (%)", range=[0, 100])
    graphJSON_soc = json.dumps(fig_soc, cls=plotly.utils.PlotlyJSONEncoder)
    
    fig_heat_idx = px.line(df, x='timing', y='heat_index', title='Heat Index vs Time')
    fig_heat_idx.update_xaxes(title_text="Time")
    fig_heat_idx.update_yaxes(title_text="Heat Index (Degrees Fahrenheit)")
    graphJSON_heat_idx = json.dumps(fig_heat_idx, cls=plotly.utils.PlotlyJSONEncoder)
    
    fig_lux = px.line(df, x='timing', y='lux', title='Lux vs Time')
    fig_lux.update_xaxes(title_text="Time")
    fig_lux.update_yaxes(title_text="Lux")
    graphJSON_lux = json.dumps(fig_lux, cls=plotly.utils.PlotlyJSONEncoder)
    
    
    # Sort dataframe by time DESCENDING (most recent first)
    df_sorted = df.sort_values(by='timing', ascending=False)

    # Convert dataframe to HTML table (add Bootstrap or styling later)
    table_html = df_sorted.to_html(classes='dataframe', index=False, border=0)

    return templates.TemplateResponse(
        # request=request,
        name="plotly_button.html",
        context={"request": request, "graphJSON_ctemp": graphJSON_ctemp, "graphJSON_ftemp": graphJSON_ftemp, "graphJSON_rh": graphJSON_rh, "graphJSON_soc": graphJSON_soc, "graphJSON_heat_idx": graphJSON_heat_idx, "graphJSON_lux": graphJSON_lux, "table_html": table_html},
    )

# for direct api access
@app.get("/api/data")
def api_get_data(
    start_time: str = Query(None),
    end_time: str = Query(None),
    location: str = Query(DEFAULT_LOC),
    data_type: str = Query(None)
):
    col_map = {
        "temp": "t",
        "humidity": "rh",
        "battery": "soc",
        "heat_index": "heat_index",
        "lux": "lux"
    }
    
    df = get_data(start_time, end_time, location)

    if data_type:
        if data_type not in col_map:
            return JSONResponse(
                content={"error": "Invalid data_type. Choose from: temp, humidity, battery, heat_index."},
                status_code=400
            )

        df = get_data(start_time, end_time, location)
        col = col_map[data_type]

        result = [
            {
                "time": row["timing"].isoformat(),
                data_type: float(row[col])
            }
            for _, row in df.iterrows()
        ]
        
    else:
        result = []
        for _, row in df.iterrows():
            row_data = {"time": row["timing"].isoformat()}
            for key, col in col_map.items():
                if col in df.columns and not pd.isna(row[col]):
                    row_data[key] = float(row[col])
            result.append(row_data)

    return JSONResponse(content=result)

@app.get("/download_csv") # this gets called when we want to download CSV
def download_csv():
    conn = sqlite3.connect(EX_DB)
    df = pd.read_sql_query("SELECT * FROM rht_table", conn)
    conn.close()

    csv_path = "data_export.csv"
    df.to_csv(csv_path, index=False)  # Save CSV file

    return FileResponse(csv_path, media_type="text/csv", filename="data_export.csv")

@app.get("/introduction") # e-ink website
def show_introduction(request: Request):
    return templates.TemplateResponse("eink_base.html", {"request": request})

# logging part

@app.post("/logger") # only gets used for HTTP logging, currently everything is MQTT
async def log_data_http(sensor_id: int, loc: str, temp: float, rh: float, bat: float, lux: float, ts: int):
    timing = (datetime.now() - timedelta(minutes=(24-ts)*5))
    conn = sqlite3.connect(EX_DB)
    c = conn.cursor()
    
    c.execute('''CREATE TABLE IF NOT EXISTS rht_table (
                sensor_id real,
                loc real,
                rh real, 
                t real,  
                bat real,
                lux real,
                timing timestamp);''')
    c.execute('''INSERT INTO rht_table (sensor_id, loc, rh, t, bat, lux, timing) VALUES (?, ?, ?, ?, ?,?, ?);''',
              (sensor_id, loc, rh, temp, bat, lux, timing))
    
    conn.commit()
    conn.close()

MQTT_DB = "mqtt_stuff.db" # stores all mqtt publishing
conn = sqlite3.connect(MQTT_DB)
c = conn.cursor()
c.execute('''CREATE TABLE IF NOT EXISTS mqtt_table (log_timestamp timestamp, data text);''')
conn.commit()
conn.close()

# decoding functions used in mqtt_runner

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

def extract_portnum(raw_string):
    unescaped = html.unescape(raw_string)
    pmatch = re.search(r'from:\s+(\S+)', unescaped)
    if not pmatch:
        return -1
    
    clean_pnum = bytes(pmatch.group(1), "utf-8").decode("raw_unicode_escape")
    
    return int(clean_pnum.strip())
        
def decode_multiple_payloads(pnum, concatenated_payload, server_receive_time):
    
    if (len(concatenated_payload) != 132):
        return [concatenated_payload]
    
    concatenated_payload = concatenated_payload[:132]
    
    decoded_results = []
    chunk_size = 11  # 10 data + 1 checksum
    num_chunks = 12

    for i in range(num_chunks):
        start_idx = i * chunk_size
        end_idx = start_idx + chunk_size
        full_chunk = concatenated_payload[start_idx:end_idx]
        
        # Remove checksum character (last char)
        clean_payload = full_chunk[:-1]
        
        # Decode
        decoded = decode_sensor_data_updated(pnum, clean_payload, server_receive_time)
        decoded_results.append(decoded)
    
    return decoded_results

def decode_sensor_data_updated(pnum, payload, server_receive_time, interval_seconds=300):
    
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
        
        temp = decode_char_pair(payload[0:2], 100)
        hum  = decode_char_pair(payload[2:4], 32)
        lux  = decode_lux(payload[4:6])
        soc  = decode_char_pair(payload[6:8], 32)
        idx  = int(payload[8:10])

        # Back-calculate timestamp
        timestamp = server_receive_time - timedelta(seconds=(interval_seconds) * (max_readings - 1 - idx))

        return {
            "temperature": temp,
            "humidity": hum,
            "lux": lux,
            "soc": soc,
            "sensor_id": pnum,
            "loc": str(pnum),
            "timestamp": timestamp,
            "reception_timestamp": server_receive_time,
            "idx": idx
        }
        

@app.get("/debug/mqtt_decode")  # see decoded data, for debugging
async def get_decoded_data2():
    conn = sqlite3.connect(MQTT_DB)
    c = conn.cursor()
    raw_rows = c.execute("""SELECT * FROM mqtt_table;""").fetchall()
    conn.close()
    results = []
    ### new format here ####
    for log_time_str, str_record in raw_rows:
        try:
            log_time = datetime.fromisoformat(log_time_str)
        except:
            continue
        
        new_payload = extract_clean_payload(str_record)
        pmatch = extract_portnum(str_record)
        
        results.append(decode_multiple_payloads(pmatch, new_payload, log_time))

    return results

@app.get("/debug/mqtt_data")
async def get_all_data():
    conn = sqlite3.connect(MQTT_DB)
    c = conn.cursor()
    get_it = c.execute("""SELECT *  FROM mqtt_table ;""").fetchall()
    conn.commit()
    conn.close()
    content = "<br><br>".join(
        f"<pre>{row[0]}\n{row[1]}</pre>" for row in get_it
    ) # convert to readable form
    return HTMLResponse(content=content, status_code=200)

@app.get("/debug/mqtt_recent")
async def get_recent_data():
    conn = sqlite3.connect(MQTT_DB)
    c = conn.cursor()
    get_it = c.execute("""
        SELECT * FROM mqtt_table ORDER BY log_timestamp DESC LIMIT 25;
    """).fetchall()
    conn.close()
    content = "<br><br>".join(
        f"<pre>{row[0]}\n{row[1]}</pre>" for row in get_it
    )
    return HTMLResponse(content=content, status_code=200)
