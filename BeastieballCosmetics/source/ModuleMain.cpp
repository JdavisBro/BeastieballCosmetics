#include <YYToolkit/YYTK_Shared.hpp>

#include "Hooks.h"
#include "ModuleMain.h"

#include <json.hpp>
#include <fstream>

using namespace Aurie;
using namespace YYTK;

#include <vector>
#include <filesystem>
#include <format>

YYTKInterface *g_ModuleInterface = nullptr;
std::vector<json> loaded_swaps;
std::map<std::string, RValue> swap_sprites;

std::string required_keys[] = {
		"type", "sprite", "condition", "id"};
std::string color_keys[] = {
		"colors", "shiny", "colors2"};

bool IsColorsValid(json colors)
{
	for (int gradientNum = 0; gradientNum < colors.size(); gradientNum++)
	{
		json gradient = colors[gradientNum];
		if (!gradient.is_array())
		{
			if (gradient.is_object() && gradient.contains("array") && gradient["array"].is_array())
			{
				gradient = gradient["array"];
			}
			else
			{
				return false;
			}
		}
		for (int colorNum = 0; colorNum < gradient.size(); colorNum++)
		{
			json color = gradient[colorNum];
			if (!color["x"].is_number())
			{
				return false;
			}
			if (!color["color"].is_number())
			{
				return false;
			}
		}
	}

	return true;
}

RValue AddSprite(json spr_data)
{
	g_ModuleInterface->PrintInfo(spr_data.dump());
	if (!spr_data["filename"].is_string())
	{
		return RValue();
	}
	std::string filename_template = spr_data["filename"].get<std::string>();
	if (!spr_data["count"].is_number())
	{
		return RValue();
	}
	int file_count = spr_data["count"].get<int>();
	if (!spr_data["origin"].is_array())
	{
		return RValue();
	}
	if (!spr_data["origin"][0].is_number() || !spr_data["origin"][1].is_number())
	{
		return RValue();
	}
	int originX = spr_data["origin"][0].get<int>();
	int originY = spr_data["origin"][1].get<int>();

	RValue working;
	g_ModuleInterface->GetBuiltin("program_directory", nullptr, NULL_INDEX, working);

	std::string path = working.ToString() + "mods/Aurie/BeastieballCosmetics/" + std::vformat(filename_template, std::make_format_args("0"));
	RValue sprite = g_ModuleInterface->CallBuiltin("sprite_add", {
																																	 RValue(path),
																																	 RValue(1),
																																	 RValue(false),
																																	 RValue(false),
																																	 RValue(484),
																																	 RValue(989),
																															 });

	RValue sprite_beastie_ball_impact = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("sprite_beastie_ball_impact")});
	RValue char_anims = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("char_animations")});

	std::string spriteStr = g_ModuleInterface->CallBuiltin("sprite_get_name", {sprite})
															.ToString();

	for (int i = 1; i < file_count; i++)
	{
		path = working.ToString() + "mods/Aurie/BeastieballCosmetics/" + std::vformat(filename_template, std::make_format_args(i));
		RValue frame = g_ModuleInterface->CallBuiltin("sprite_add", {
																																		RValue(path),
																																		RValue(1),
																																		RValue(false),
																																		RValue(false),
																																		RValue(484),
																																		RValue(989),
																																});
		g_ModuleInterface->CallBuiltin("sprite_merge", {sprite, frame});
		g_ModuleInterface->CallBuiltin("sprite_delete", {frame});
	}

	if (spr_data["animations"].is_string())
	{
		std::string oldSpriteStr = spr_data["animations"].get<std::string>();
		RValue oldImpact = g_ModuleInterface->CallBuiltin("variable_struct_get", {sprite_beastie_ball_impact, RValue("_" + oldSpriteStr)});
		g_ModuleInterface->CallBuiltin("variable_struct_set", {sprite_beastie_ball_impact,
																													 RValue("_" + spriteStr),
																													 oldImpact});

		RValue oldSprite = g_ModuleInterface->CallBuiltin("asset_get_index", {RValue(oldSpriteStr)});
		RValue oldAnimations = g_ModuleInterface->CallBuiltin("ds_map_find_value", {char_anims,
																																								oldSprite});
		g_ModuleInterface->CallBuiltin("ds_map_set", {char_anims, sprite, oldAnimations});
	}

	return sprite;
}

void AddSwap(json data, std::string FileName)
{
	for (std::string key : required_keys)
	{
		if (!data.contains(key))
		{
			g_ModuleInterface->PrintWarning("Error Loading %s - Missing Key %s", FileName, key);
			return;
		}
	}
	if (!data["id"].is_string())
	{
		g_ModuleInterface->PrintWarning("Error Loading %s", FileName);
		return;
	}
	std::string id = data["id"].get<std::string>();

	json sprite = data["sprite"];
	if (sprite.is_string())
	{
		g_ModuleInterface->PrintInfo("sprite %s", sprite.get<std::string>());
		RValue spriteRef = g_ModuleInterface->CallBuiltin("asset_get_index", {RValue(sprite.get<std::string>())});
		swap_sprites[id] = spriteRef;
	}
	else
	{
		g_ModuleInterface->PrintInfo(sprite.dump());
		if (!sprite.is_object())
		{
			g_ModuleInterface->PrintWarning("Error Loading %s - Invalid Sprite", FileName);
			return;
		}
		RValue spriteRef = AddSprite(sprite);
		if (!spriteRef.ToBoolean())
		{
			g_ModuleInterface->PrintWarning("Error Loading %s - Invalid Sprite", FileName);
			return;
		}
		swap_sprites[id] = spriteRef;
	}

	RValue char_dic = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("char_dic")});
	for (std::string key : color_keys)
	{
		if (!data.contains(key))
		{
			continue;
		}
		json color = data[key];
		if (color.is_string())
		{
			RValue beastie = g_ModuleInterface->CallBuiltin("ds_map_find_value", {char_dic, RValue(color.get<std::string>())});
			RValue coldata = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue(key)});
			data[key] = json::parse(g_ModuleInterface->CallBuiltin("json_stringify", {coldata}).ToString());
		}
		if (!data[key].is_array() || !IsColorsValid(data[key]))
		{
			data[key] = json{};
		}
	}
	g_ModuleInterface->PrintInfo(data.dump());
	loaded_swaps.push_back(data);
}

void LoadSwaps()
{
	g_ModuleInterface->Print(CM_LIGHTGREEN, "[BeastieballCosmetics] - Loading Swaps!");
	std::filesystem::path dir = {"mods"};
	dir = dir / "Aurie" / "BeastieballCosmetics";
	std::filesystem::create_directories(dir);
	for (auto const &dir_entry : std::filesystem::directory_iterator(dir))
	{
		if (dir_entry.is_directory())
		{
			for (auto const &subdir_entry : std::filesystem::directory_iterator(dir_entry))
			{
				if (subdir_entry.is_regular_file() && subdir_entry.path().extension() == ".json")
				{
					g_ModuleInterface->PrintInfo(subdir_entry.path().string());
					std::fstream f(subdir_entry.path());
					json data = json::parse(f);
					AddSwap(data, subdir_entry.path().filename().string());
				}
			}
		}
	}
}

void FrameCallback(FWFrame &FrameContext)
{
	UNREFERENCED_PARAMETER(FrameContext);

	AurieStatus last_status = AURIE_SUCCESS;

	static uint32_t frame_counter = 0;

	if (frame_counter > 100)
	{
		return;
	}

	if (frame_counter == 100)
	{
		try
		{
			LoadSwaps();
		}
		catch (const json::exception &e)
		{
			g_ModuleInterface->PrintWarning("eror!! %s", e.what());
		}
	}
	frame_counter++;
}

EXPORTED AurieStatus
ModuleInitialize(
		IN AurieModule *Module,
		IN const fs::path &ModulePath)
{
	UNREFERENCED_PARAMETER(ModulePath);

	AurieStatus last_status = AURIE_SUCCESS;

	// Gets a handle to the interface exposed by YYTK
	// You can keep this pointer for future use, as it will not change unless YYTK is unloaded.
	g_ModuleInterface = YYTK::GetInterface();

	// If we can't get the interface, we fail loading.
	if (!g_ModuleInterface)
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

	CreateAllHooks();

	last_status = g_ModuleInterface->CreateCallback(
			Module,
			EVENT_FRAME,
			FrameCallback,
			0);

	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->Print(CM_LIGHTRED, "[BeastieballCosmetics] - Failed to register frame callback!");
	}

	g_ModuleInterface->Print(CM_LIGHTGREEN, "[BeastieballCosmetics] - Hello from PluginEntry!");

	return AURIE_SUCCESS;
}
