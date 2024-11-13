@REM Remove these settings to reinstall SmatConfig wifi mode

pushd %~dp0..
set HBF_WIFI_SSID=
set HBF_WIFI_PASW=
set HBF_WS_HOST=

@REM A rebuild is needed
idf.py fullclean
popd
