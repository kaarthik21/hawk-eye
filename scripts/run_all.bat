@echo off
echo 🔁 Starting Hawk-Eye System...

start "" bin\feed-simulator.exe
start "" bin\spoofing-detector.exe
start "" bin\quote-stuffing.exe
start "" bin\price-deviation.exe

start "" cmd /k "cd genai && call venv\Scripts\activate.bat && pip install -r requirements.txt && python summarize_alerts.py"
