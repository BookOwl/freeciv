@echo off
if %1 NEQ auto set LANG=%1
start freeciv-mp-gtk3x.exe %2 %3 %4 %5 %6 %7 %8 %9
