import requests
from bs4 import BeautifulSoup
from datetime import datetime
import re

# 1) GET the page
resp = requests.get('https://sailing.mit.edu/weather/', timeout=10)
resp.raise_for_status()

# 2) Extract text lines
soup  = BeautifulSoup(resp.text, 'html.parser')
text  = soup.get_text('\n')
lines = [ln.strip() for ln in text.splitlines() if ln.strip()]

# 3) Find Temperature, Humidity, Solar radiation
temp = hum = sol_rad = None
for i, ln in enumerate(lines):
    if ln.startswith("Temperature"):
        temp = float(lines[i+1].split()[0])
    elif ln.startswith("Humidity"):
        hum = float(lines[i+1].split()[0])
    elif ln.startswith("Solar radiation"):
        # grab first number on this same line
        m = re.search(r'(\d+(?:\.\d+)?)', ln)
        sol_rad = float(m.group(1)) if m else float(lines[i+1].split()[0])
    if temp is not None and hum is not None and sol_rad is not None:
        break

# 4) Print the results
now = datetime.utcnow().isoformat()
print(f"Time:            {now}")
print(f"Temperature:     {temp} °F")
print(f"Humidity:        {hum} %")
print(f"Solar radiation: {sol_rad} W/m²")
