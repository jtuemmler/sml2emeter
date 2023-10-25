@echo on

set MOSQUITTO_DIR=c:\Program Files\mosquitto

start "Mosquitto" /min "%MOSQUITTO_DIR%\mosquitto" -v -c mosquitto.conf

"%MOSQUITTO_DIR%\mosquitto_sub" -v -t "#"
