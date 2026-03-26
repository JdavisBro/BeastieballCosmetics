#!/bin/bash
"/c/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" /property:Configuration=Release
cp "x64/Release/BeastieballCosmetics.dll" "$beastieball/mods/aurie/"
