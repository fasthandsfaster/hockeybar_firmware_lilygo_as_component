@echo off

pushd %~dp0..

cls
@REM if [%IDF_PATH%] == [] call e:\udv\esp32\esp-idf-v4.4\export.bat
if [%IDF_PATH%] == [] call e:\udv\esp32\esp-idf-v5.0\export.bat

idf.py -p com6 flash monitor

popd
