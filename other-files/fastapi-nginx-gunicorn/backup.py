from typing import Annotated
from fastapi import FastAPI, Query
from fastapi.responses import HTMLResponse
from fastapi.responses import JSONResponse
from fastapi import Request
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from fastapi.responses import FileResponse
templates = Jinja2Templates(directory="templates")

import pandas as pd
import plotly
import plotly.express as px
import json

import math

import sqlite3
from datetime import datetime

app = FastAPI()

EX_DB = "rachcai_test/weekly_log.db"
DEFAULT_START = "2025-03-05 00:00:00"
DEFAULT_END = "2025-03-13 23:59:59"

@app.get("/")
def home(request: Request):
    return templates.TemplateResponse("plotly_button.html", {"request": request})

def get_data(start_time, end_time):
    conn = sqlite3.connect(EX_DB)
    query = """
        SELECT timing, t, rh, bat 
        FROM rht_table 
        WHERE timing BETWEEN ? AND ?
        ORDER BY timing DESC;
    """
    start_time_dt = datetime.strptime(start_time, "%Y-%m-%d %H:%M:%S")
    end_time_dt = datetime.strptime(end_time, "%Y-%m-%d %H:%M:%S")
    df = pd.read_sql_query(query, conn, params=(start_time_dt, end_time_dt))
    conn.close()
    df['timing'] = pd.to_datetime(df['timing'])
    df['soc'] = (df['bat'] * 100) / 4.2
    return df

@app.get("/show_plots")
def get_plots(request: Request, start_time: str = Query(DEFAULT_START), end_time: str = Query(DEFAULT_END)):   
    df = get_data(start_time, end_time)
    
    fig_temp = px.line(df, x='timing', y='t', title='Temperature vs Time')
    fig_temp.update_xaxes(title_text="Time")
    fig_temp.update_yaxes(title_text="Temperature (Degrees)")
    graphJSON_temp = json.dumps(fig_temp, cls=plotly.utils.PlotlyJSONEncoder)

    fig_rh = px.line(df, x='timing', y='rh', title='Relative Humidity vs Time')
    fig_rh.update_xaxes(title_text="Time")
    fig_rh.update_yaxes(title_text="Relative Humidity (%)")
    graphJSON_rh = json.dumps(fig_rh, cls=plotly.utils.PlotlyJSONEncoder)

    fig_soc = px.line(df, x='timing', y='soc', title='State of Charge vs Time')
    fig_soc.update_xaxes(title_text="Time")
    fig_soc.update_yaxes(title_text="SOC (%)")
    graphJSON_soc = json.dumps(fig_soc, cls=plotly.utils.PlotlyJSONEncoder)

    return templates.TemplateResponse(
            #request=request,
        name="plotly_button.html",
        context={"request": request,"graphJSON_temp": graphJSON_temp, "graphJSON_rh": graphJSON_rh, "graphJSON_soc": graphJSON_soc},
    )


@app.get("/temp_plot")
def temp_plot(request: Request, start_time: str = Query(DEFAULT_START), end_time: str = Query(DEFAULT_END)):   
    df = get_data(start_time, end_time)
    fig = px.line(df, x='timing', y='t', title='Temperature vs Time')
    fig.update_xaxes(title_text="Time")
    fig.update_yaxes(title_text="Temperature (Degrees)")
    graphJSON = json.dumps(fig, cls=plotly.utils.PlotlyJSONEncoder)
    return templates.TemplateResponse(request=request, name="plotly_button.html", context={"graphJSON": graphJSON})


@app.get("/rh_plot")
def rh_plot(request: Request, start_time: str = Query(DEFAULT_START), end_time: str = Query(DEFAULT_END)):
    df = get_data(start_time, end_time)
    fig = px.line(df, x='timing', y='rh', title='Relative Humidity vs Time')
    fig.update_xaxes(title_text="Time")
    fig.update_yaxes(title_text="Relative Humidity (%)")
    graphJSON = json.dumps(fig, cls=plotly.utils.PlotlyJSONEncoder)
    return templates.TemplateResponse(request=request, name="plotly_base.html", context={"graphJSON": graphJSON})


@app.get("/soc_plot")
def soc_plot(request: Request, start_time: str = Query(DEFAULT_START), end_time: str = Query(DEFAULT_END)):
    df = get_data(start_time, end_time)
    fig = px.line(df, x='timing', y='soc', title='State of Charge vs Time')
    fig.update_xaxes(title_text="Time")
    fig.update_yaxes(title_text="SOC (%)")
    graphJSON = json.dumps(fig, cls=plotly.utils.PlotlyJSONEncoder)
    return templates.TemplateResponse(request=request, name="plotly_base.html", context={"graphJSON": graphJSON})

@app.get("/download_csv")
def download_csv():
    conn = sqlite3.connect(EX_DB)
    df = pd.read_sql_query("SELECT * FROM rht_table", conn)
    conn.close()

    csv_path = "data_export.csv"
    df.to_csv(csv_path, index=False)  # Save CSV file

    return FileResponse(csv_path, media_type="text/csv", filename="data_export.csv")
