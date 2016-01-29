#!/usr/bin/env bash
set -eu

DISPLAY=:0
#Xephyr -ac -br -reset -terminate -screen 800x600 :1 &
Xephyr -ac -br -noreset -screen 800x600 :1 &
DISPLAY=:1
