@echo off
pushd %~dp0

start "HBF_WS" node ws-test-server.js

popd
