from fastapi import FastAPI, Query, Request
import sqlite3
import datetime
from pydantic import BaseModel

# app = FastAPI()
 
app = FastAPI(
    title="EFI Development",
    openapi_url=f"/openapi.json",
    docs_url=f"/docs",
    redoc_url=f"/redoc",
    root_path="/tunnel_gpcs" 
)
 
def compute_heat_index(rh: float, t: float) -> int:
  """
  Calculate the heat index based on temperature (t) and relative humidity (rh).
  Returns the heat index rounded to the nearest integer.
  """
  if t < 80:
      return round(t)  # Heat index is only valid for temperatures >= 80°F
  
  hi = (-42.379 +
        2.04901523 * t +
        10.14333127 * rh -
        0.22475541 * t * rh -
        6.83783e-3 * t**2 -
        5.481717e-2 * rh**2 +
        1.22874e-3 * t**2 * rh +
        8.5282e-4 * t * rh**2 -
        1.99e-6 * t**2 * rh**2)
  
  return round(hi)

RHT_DB = "sensor_log.db"  # you'd obvi change this

@app.post("/logger")
def log_sensor_data(temp: float, rh: float, bat: float, ts : int):

    # Estimate real timestamp based on RTC ts (every 5 minutes)
    current_time = (datetime.datetime.now() - datetime.timedelta(minutes=ts * 5)).replace(microsecond=0)

    heat_index = compute_heat_index(rh, temp)

    conn = sqlite3.connect(RHT_DB)
    c = conn.cursor()
    # the table exists
    c.execute('''CREATE TABLE IF NOT EXISTS sensor_log (
                    temp REAL, 
                    rh REAL, 
                    heat_index REAL, 
                    bat REAL, 
                    timestamp TEXT
                );''')

    # insert the new sensor data
    c.execute('''INSERT INTO sensor_log (temp, rh, heat_index, bat, timestamp) 
                 VALUES (?, ?, ?, ?, ?);''', (temp, rh, heat_index, bat, current_time))

    conn.commit()
    conn.close()

    return {
        "status": "success",
        "logged": {
            "temp": temp,
            "rh": rh,
            "heat_index": heat_index,
            "bat": bat,
            "timestamp": current_time
        }
    }
    
@app.get("/get_data")
def get_sensor_data(time: int):
   
    cutoff_time = datetime.datetime.now() - datetime.timedelta(minutes=time)
    cutoff_time_str = cutoff_time.isoformat()

    conn = sqlite3.connect(RHT_DB)
    c = conn.cursor()

    # get data from the last `time` minutes
    prev_data = c.execute('''SELECT temp, rh, heat_index, bat, timestamp FROM sensor_log 
                            WHERE timestamp >= ? 
                            ORDER BY timestamp DESC;''', (cutoff_time_str,)).fetchall()

    conn.commit()
    conn.close()

    if not prev_data:
        return {"message": "No data found within the specified time range."}

    return [{"timestamp": row[4], "temp": row[0], "rh": row[1], "heat_index": row[2], "bat": row[3]} for row in prev_data]

ERROR_DB = "error_log.db"

class ErrorLogRequest(BaseModel):
    error_type: str
    error_msg: str

@app.post("/error_logger")
def log_error(request: ErrorLogRequest):

    current_time = datetime.datetime.now().isoformat()

    conn = sqlite3.connect(ERROR_DB)
    c = conn.cursor()

    # Ensure the table exists
    c.execute('''CREATE TABLE IF NOT EXISTS error_log (
                    error_type TEXT, 
                    error_msg TEXT, 
                    timestamp TEXT
                );''')
    conn.commit()  

    # Insert error log
    c.execute('''INSERT INTO error_log (error_type, error_msg, timestamp) 
                 VALUES (?, ?, ?);''', (request.error_type, request.error_msg, current_time))

    conn.commit()
    conn.close()

    return {"status": "error logged", "error_type": request.error_type, "message": request.error_msg, "timestamp": current_time}

@app.get("/get_errors")
def get_errors(time: int):

    cutoff_time = datetime.datetime.now() - datetime.timedelta(minutes=time)
    cutoff_time_str = cutoff_time.isoformat()

    conn = sqlite3.connect(ERROR_DB)
    c = conn.cursor()

    prev_errors = c.execute('''SELECT error_type, error_msg, timestamp FROM error_log 
                               WHERE timestamp >= ? 
                               ORDER BY timestamp DESC;''', (cutoff_time_str,)).fetchall()

    conn.commit()
    conn.close()

    if not prev_errors:
        return {"message": "No errors found within the specified time range."}

    return [{"timestamp": row[2], "error_type": row[0], "message": row[1]} for row in prev_errors]
