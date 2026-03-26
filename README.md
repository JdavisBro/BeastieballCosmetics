# BeastieballCosmetics

Mod created with [YYToolkit](https://github.com/AurieFramework/YYToolkit) to add cosmetic customizations to Beastieball!

## Installation

- Go to beastieball directory (you should see `beastieball.exe`)
- Make `mods` folder
- Inside `mods` folder make `aurie` and `native` folder
- from https://github.com/AurieFramework/Aurie/releases/
  - get `AuriePatcher.exe` and put it in the `mods` folder
  - put `AurieCore.dll` into the `native` folder
- from https://github.com/AurieFramework/YYToolkit/releases/ get `YYToolkit.dll` and put it in the `aurie` folder
- Download "BeastieballCosmetics.dll" from [releases](https://github.com/JdavisBro/BeastieballCosmetics/releases) and put that in the `aurie` folder
- from the game's dir
  - run this to install `mods\AuriePatcher.exe beastieball.exe mods\native\AurieCore.dll install`
  - run this to uninstall `mods\AuriePatcher.exe beastieball.exe mods\native\AurieCore.dll remove`
- run `beastieball.exe` or open it through steam

## Adding Swaps

Beastie sprite swaps are stored in `mod_data/BeastieballCosmetics` (from beastieball.exe). JSON files are searched for in that directory, and subdirs of that directory. So `BeastieballCosmetics/example.json` and `BeastieballCosmetics/examples/example.json` would be loaded, but `BeastieballCosmetics/example/example1/example.json` would not.

Example BeastieCosmetics dir:

- purpleSprecko
  - purpleSprecko.json
- spreckoHat
  - 0.png
  - 1.png
  - ...
  - 35.png
  - hat.json

## Creating Swaps

See the [Docs](https://JdavisBro.github.io/BeastieballCosmetics/) for more info on how Swap JSON are formatted and how the beastie shader works.
