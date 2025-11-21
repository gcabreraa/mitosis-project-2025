import sqlite3
from datetime import datetime, timedelta

# DB_PATH    = 'stata_test_0416.db'
DB_PATH = 'stata_test_log_041025_night.db'
TABLE_NAME = 'rht_table'
BASE_TS    = '2025-04-16 15:27:23'  # latest bad timestamp

# 1. Connect
conn   = sqlite3.connect(DB_PATH)
cursor = conn.cursor()

# 2. Grab all the mis‑timestamped rows, newest first
cursor.execute(f"""
    SELECT timing
      FROM {TABLE_NAME}
     WHERE timing <= ? AND loc='stata_416'
  ORDER BY timing DESC
""", (BASE_TS,))
old_timestamps = [row[0] for row in cursor.fetchall()]

# 3. Loop and update by matching on the old timestamp string
for i, old_str in enumerate(old_timestamps):
    old_ts = datetime.strptime(old_str, '%Y-%m-%d %H:%M:%S.%f')
    # add back 5*i seconds, subtract 5*i minutes
    new_ts = old_ts + timedelta(seconds=5*i) - timedelta(minutes=5*i)
    new_str = new_ts.strftime('%Y-%m-%d %H:%M:%S.%f')

    cursor.execute(f"""
        UPDATE {TABLE_NAME}
           SET timing = ?
         WHERE timing = ?
    """, (new_str, old_str))

# 4. Commit & close
conn.commit()
conn.close()
