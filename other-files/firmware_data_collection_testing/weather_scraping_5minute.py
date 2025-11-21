import sqlite3
import requests
from bs4 import BeautifulSoup
from datetime import datetime
from apscheduler.schedulers.blocking import BlockingScheduler
import re

# ——————————————
# 1) DATABASE SETUP
# ——————————————
DB_PATH = 'weather_minute.db'
TABLE   = 'weather_readings'

conn = sqlite3.connect(DB_PATH)
c = conn.cursor()
c.execute(f"""
CREATE TABLE IF NOT EXISTS {TABLE} (
    logged_at        TEXT PRIMARY KEY,   -- ISO timestamp of scrape
    temperature      REAL,              -- °F
    humidity         REAL,              -- %
    solar_radiation  REAL               -- W/m²
)
""")
conn.commit()
conn.close()


# ——————————————
# 2) SCRAPE + STORE FUNCTION
# ——————————————
def fetch_and_store():
    # 1) GET the page
    resp = requests.get('https://sailing.mit.edu/weather/', timeout=10)
    resp.raise_for_status()

    # 2) Pull out all lines of text
    soup  = BeautifulSoup(resp.text, 'html.parser')
    text  = soup.get_text('\n')
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]

    # 3) Parse metrics
    temp = hum = sol_rad = None
    for i, ln in enumerate(lines):
        if ln.startswith("Temperature"):
            temp = float(lines[i+1].split()[0])
        elif ln.startswith("Humidity"):
            hum = float(lines[i+1].split()[0])
        elif ln.startswith("Solar radiation"):
            m = re.search(r'(\d+(?:\.\d+)?)', ln)
            sol_rad = float(m.group(1)) if m else float(lines[i+1].split()[0])
        if temp is not None and hum is not None and sol_rad is not None:
            break

    if None in (temp, hum, sol_rad):
        print(" Parsing failure — skipping this cycle")
        return

    # 4) Insert into DB
    now = datetime.utcnow().isoformat()
    conn = sqlite3.connect(DB_PATH)
    c    = conn.cursor()
    c.execute(f"""
        INSERT OR REPLACE INTO {TABLE}
            (logged_at, temperature, humidity, solar_radiation)
        VALUES (?, ?, ?, ?)
    """, (now, temp, hum, sol_rad))
    conn.commit()
    conn.close()

    print(f"[{now}]  Temp={temp}°F, Humidity={hum}%, SolarRad={sol_rad}W/m²")


# ——————————————
# 3) SCHEDULER (every 1 minute)
# ——————————————
if __name__ == '__main__':
    # Run once immediately
    fetch_and_store()

    # Then schedule every 1 minute
    scheduler = BlockingScheduler()
    scheduler.add_job(fetch_and_store, 'interval', minutes=1)
    print("⏱  Scraping every 5 minute…")
    scheduler.start()
