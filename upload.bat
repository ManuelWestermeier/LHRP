@echo off
REM List of COM ports
set PORTS=3 5 7

REM Loop over each port
for %%P in (%PORTS%) do (
    echo Uploading to COM%%P...
    pio run --target upload --upload-port COM%%P
    echo.
)

echo Done!
cls