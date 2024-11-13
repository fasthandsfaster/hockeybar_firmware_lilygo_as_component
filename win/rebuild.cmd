pushd %~dp0..
idf.py fullclean
call win\flash-and-monitor.cmd
popd
