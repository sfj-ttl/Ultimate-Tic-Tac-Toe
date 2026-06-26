@echo off
setlocal
cd /d "%~dp0"
if not exist build mkdir build
pushd build
where g++ >nul 2>nul
if %errorlevel%==0 (
  g++ -std=c++17 -O2 ..\backend_template\main.cpp -lws2_32 -o ultimate_ttt_server.exe
  if errorlevel 1 goto failed
  echo Starting backend and opening http://localhost:8080 ...
  start "" http://localhost:8080
  ultimate_ttt_server.exe
  goto done
)
where cl >nul 2>nul
if %errorlevel%==0 (
  cl /EHsc /std:c++17 ..\backend_template\main.cpp ws2_32.lib /Fe:ultimate_ttt_server.exe
  if errorlevel 1 goto failed
  echo Starting backend and opening http://localhost:8080 ...
  start "" http://localhost:8080
  ultimate_ttt_server.exe
  goto done
)
echo Could not find g++ or cl. Install MinGW g++ or run from Visual Studio Developer Command Prompt.
goto done
:failed
echo Build failed.
:done
popd
pause
