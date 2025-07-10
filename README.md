# BeastieballCosmetics

Mod created with [YYToolkit](https://github.com/AurieFramework/YYToolkit) to add cosmetic customizations to Beastieball!

## Installation

- Download "AurieManager.exe" from [releases](https://github.com/AurieFramework/Aurie/releases) and run it
- Select "Add Game" and find beastieball.exe (usually at `C:\Program Files (x86)\Steam\steamapps\common\Beastieball\beastieball.exe`)
- Select "Install Aurie"
- Download "BeastieballCosmetics.dll" from [releases](https://github.com/JdavisBro/BeastieballCosmetics/releases)
- Select "Add Mods" and choose "BeastieballCosmetics.dll"

Now when you run the game (either by openning the exe, through Steam, or clicking the button in AurieManager) it'll pop-up with the YYTK console window. It'll take a while to load and might close immediately, just try again.

> Note!!! To uninstall aurie you have to do it through AurieManager.exe by pressing the "Uninstall Aurie" button or the game won't be able to launch.

## Adding Swaps

Beastie sprite swaps are stored in `mods/Aurie/BeastieballCosmetics` (from beastieball.exe). JSON files are searched for in subdirs of this directory. So `BeastieballCosmetics/example.json` wouldn't be loaded, but `BeastieballCosmetics/examples/example.json` would be loaded.

Example BeastieCosmetics dir:

- purpleSprecko
  - purpleSprecko.json
- spreckoHat
  - 0.png
  - 1.png
  - ...
  - 35.png
  - hat.json
