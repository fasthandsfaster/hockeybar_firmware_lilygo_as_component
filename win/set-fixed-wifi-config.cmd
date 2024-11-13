@REM Use these settings to enable fixed SSID mode
@REM Set them to match your own wifi and IP
@REM To change back to SmartConfig remove HBF_WIFI_SSID

pushd %~dp0..
set HBF_WIFI_SSID=NETGEAR27
set HBF_WIFI_PASW=littletable200
set HBF_WS_HOST=192.168.0.26:8080

@REM A rebuild is needed
idf.py fullclean
popd