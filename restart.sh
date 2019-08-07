#!/bin/bash

killall -s SIGQUIT currency_server_helper.exe
sleep 1
./currency_server_helper.exe config.json
