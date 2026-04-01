@echo off
if not exist "build\msvc\voxpopuli.sln" (
    echo Configuring...
    cmake --preset msvc
)
start "" "voxpopuli.sln"