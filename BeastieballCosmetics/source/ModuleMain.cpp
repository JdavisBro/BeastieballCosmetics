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

AurieStatus GetScript(std::string FunctionName, CScript *&script)
{
	AurieStatus last_status = AURIE_SUCCESS;
	int index;
	last_status = g_ModuleInterface->GetNamedRoutineIndex(FunctionName.c_str(), &index);
	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintWarning("Failed to get index for %s", FunctionName);
		return last_status;
	}
	last_status = g_ModuleInterface->GetScriptData(index - 100000, script);
	if (!AurieSuccess(last_status))
	{
		g_ModuleInterface->PrintWarning("Failed to get data for %s", FunctionName);
	}
	return last_status;
}

PFUNC_YYGMLScript elephantFromJson = nullptr;

std::vector<json> loaded_swaps;
std::map<std::string, RValue> swap_sprites;
std::map<std::string, RValue> swap_loco;

std::string required_keys[] = {
		"type", "id"};
std::string color_keys[] = {
		"colors", "shiny", "colors2"};

bool IsColorsValid(json colors)
{
	if (!colors.is_array() || !colors.size())
	{
		return false;
	}
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
		if (!gradient.size())
		{
			return false;
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

RValue AddSprite(json spr_data, std::string id)
{
	g_ModuleInterface->PrintInfo(std::format("[BeastieballCosmetics] - Loading Sprites for {}...", id));
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
	int originX = 484;
	int originY = 989;
	if (spr_data["origin"].is_array() && spr_data["origin"][0].is_number() && spr_data["origin"][1].is_number())
	{
		originX = spr_data["origin"][0].get<int>();
		originY = spr_data["origin"][1].get<int>();
	}

	RValue working;
	g_ModuleInterface->GetBuiltin("program_directory", nullptr, NULL_INDEX, working);

	std::string path = working.ToString() + "mod_data/BeastieballCosmetics/" + std::vformat(filename_template, std::make_format_args("0"));
	if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
	{
		g_ModuleInterface->PrintInfo(std::format("Error loading Sprite {}, file not found {}", id, path));
		return RValue();
	}
	RValue sprite = g_ModuleInterface->CallBuiltin("sprite_add", {
																																	 RValue(path),
																																	 RValue(1),
																																	 RValue(false),
																																	 RValue(false),
																																	 RValue(originX),
																																	 RValue(originY),
																															 });

	std::string spriteStr = g_ModuleInterface->CallBuiltin("sprite_get_name", {sprite})
															.ToString();

	for (int i = 1; i < file_count; i++)
	{
		path = working.ToString() + "mod_data/BeastieballCosmetics/" + std::vformat(filename_template, std::make_format_args(i));
		if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
		{
			g_ModuleInterface->PrintInfo(std::format("Error loading Sprite {}, file not found {}", id, path));
			g_ModuleInterface->CallBuiltin("sprite_delete", {sprite});
			return RValue();
		}
		RValue frame = g_ModuleInterface->CallBuiltin("sprite_add", {
																																		RValue(path),
																																		RValue(1),
																																		RValue(false),
																																		RValue(false),
																																		RValue(originX),
																																		RValue(originY),
																																});
		g_ModuleInterface->CallBuiltin("sprite_merge", {sprite, frame});
		g_ModuleInterface->CallBuiltin("sprite_delete", {frame});
	}

	g_ModuleInterface->PrintInfo(std::format("[BeastieballCosmetics] - Loaded Sprites for {}!", id));

	json json_animations = spr_data["animations"];

	RValue sprite_beastie_ball_impact = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("sprite_beastie_ball_impact")});
	RValue char_anims = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("char_animations")});

	RValue impact;
	if (json_animations.is_string())
	{
		std::string oldSpriteStr = json_animations.get<std::string>();
		impact = g_ModuleInterface->CallBuiltin("variable_struct_get", {sprite_beastie_ball_impact, RValue("_" + oldSpriteStr)});
	}
	else if (json_animations.is_object())
	{
		RValue parsed = g_ModuleInterface->CallBuiltin("json_parse", {RValue(json_animations.dump())});
		RValue *args[] = {&parsed};
		elephantFromJson(nullptr, nullptr, impact, 1, args);
	}

	if (!impact.ToBoolean())
	{
		g_ModuleInterface->PrintInfo(std::format("Error Loading Sprite Animations. {}", id));
		g_ModuleInterface->CallBuiltin("sprite_delete", {sprite});
		return RValue();
	}
	RValue animations = g_ModuleInterface->CallBuiltin("variable_struct_get", {impact, RValue("anim_data")});
	RValue loco = g_ModuleInterface->CallBuiltin("variable_struct_get", {impact, RValue("loco_data")});
	if (!animations.ToBoolean() || !loco.ToBoolean())
	{
		g_ModuleInterface->PrintInfo(std::format("Error Loading Sprite Animations. {}", id));
		g_ModuleInterface->CallBuiltin("sprite_delete", {sprite});
		return RValue();
	}
	g_ModuleInterface->CallBuiltin("variable_struct_set", {sprite_beastie_ball_impact,
																												 RValue("_" + spriteStr),
																												 impact});
	g_ModuleInterface->CallBuiltin("ds_map_set", {char_anims, sprite, animations});
	swap_loco[id] = loco;

	return sprite;
}

void AddSwap(std::filesystem::path path)
{
	std::string fileName = path.stem().string();
	std::fstream f(path);
	json data = json::parse(f);
	for (std::string key : required_keys)
	{
		if (!data.contains(key))
		{
			g_ModuleInterface->PrintWarning("Error Loading %s - Missing Key %s", fileName, key);
			return;
		}
	}
	if (!data["id"].is_string())
	{
		g_ModuleInterface->PrintWarning("Error Loading %s", fileName);
		return;
	}
	std::string id = data["id"].get<std::string>();
	g_ModuleInterface->PrintInfo(std::format("[BeastieballCosmetics] - Loading {}...", id));

	json sprite = data["sprite"];
	if (sprite.is_null())
	{
	}
	else if (sprite.is_string())
	{
		std::string sprName = sprite.get<std::string>();
		RValue spriteRef = g_ModuleInterface->CallBuiltin("asset_get_index", {RValue(sprName)});
		if (!spriteRef.ToBoolean())
		{
			g_ModuleInterface->PrintWarning(std::format("Error Loading {} - Invalid Sprite Name {}", fileName, sprName));
			return;
		}

		RValue sprite_beastie_ball_impact = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("sprite_beastie_ball_impact")});
		RValue impact = g_ModuleInterface->CallBuiltin("variable_struct_get", {sprite_beastie_ball_impact, RValue("_" + sprName)});
		if (!impact.ToBoolean())
		{
			g_ModuleInterface->PrintWarning(std::format("Error Loading {} - Sprite {} has no Beastie anims", fileName, sprName));
			return;
		}
		RValue loco = g_ModuleInterface->CallBuiltin("variable_struct_get", {impact, RValue("loco_data")});
		RValue char_anims = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("char_animations")});
		RValue char_anims_sprite = g_ModuleInterface->CallBuiltin("ds_map_get", {char_anims, spriteRef});
		if (!char_anims_sprite.ToBoolean()) // some beastie sprites (alt sprites like sprRat_alt) aren't in char_animations
		{
			RValue animations = g_ModuleInterface->CallBuiltin("variable_struct_get", {impact, RValue("anim_data")});
			g_ModuleInterface->CallBuiltin("ds_map_set", {char_anims, spriteRef, animations});
		}

		swap_sprites[id] = spriteRef;
		swap_loco[id] = loco;
	}
	else
	{
		if (!sprite.is_object())
		{
			g_ModuleInterface->PrintWarning("Error Loading %s - Invalid Sprite", fileName);
			return;
		}
		RValue spriteRef = AddSprite(sprite, id);
		if (!spriteRef.ToBoolean())
		{
			g_ModuleInterface->PrintWarning("Error Loading %s - Invalid Sprite", fileName);
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
			if (beastie.ToBoolean())
			{
				RValue coldata = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue(key)});
				data[key] = json::parse(g_ModuleInterface->CallBuiltin("json_stringify", {coldata}).ToString());
			}
			else
			{
				g_ModuleInterface->PrintWarning(std::format("Error Loading {} - {} has beastie {} which doesn't exist", fileName, key, color.get<std::string>()));
			}
		}
		if (!IsColorsValid(data[key]))
		{
			g_ModuleInterface->PrintWarning(std::format("Error Loading {} - {} invalid format.", fileName, key));
			data[key] = json{};
		}
	}
	g_ModuleInterface->PrintInfo(std::format("[BeastieballCosmetics] - Loaded {}", id));
	loaded_swaps.push_back(data);
}

int GetConditionValue(json condition)
{
	if (!condition.is_object())
	{
		return 0;
	}
	int value = 0;
	if (condition["specie"].is_string())
	{
		value += 1;
	}
	if (condition["names"].is_array() && condition["names"].size())
	{
		value += 2 + (int)condition["names"].size();
	}
	return value;
}

bool SwapCompare(json const swap1, json const swap2)
{
	std::string id1 = swap1["id"].get<std::string>();
	std::string id2 = swap2["id"].get<std::string>();
	// should prioritise swaps with more conditions to not be overwritten by swaps with less conditions.
	int cond1 = GetConditionValue(swap1["condition"]);
	int cond2 = GetConditionValue(swap2["condition"]);
	return cond1 != cond2 ? cond1 > cond2 : id1.compare(id2) >= 0;
}

void LoadSwaps()
{
	g_ModuleInterface->Print(CM_LIGHTGREEN, "[BeastieballCosmetics] - Loading Swaps!");

	CScript *script = nullptr;
	AurieStatus last_status = GetScript("gml_Script_ElephantFromJSON", script);
	elephantFromJson = script->m_Functions->m_ScriptFunction;

	std::filesystem::path dir = {"mod_data"};
	dir = dir / "BeastieballCosmetics";
	std::filesystem::create_directories(dir);
	for (auto const &dir_entry : std::filesystem::directory_iterator(dir))
	{
		if (dir_entry.is_directory())
		{
			for (auto const &subdir_entry : std::filesystem::directory_iterator(dir_entry))
			{
				if (subdir_entry.is_regular_file() && subdir_entry.path().extension() == ".json")
				{
					AddSwap(subdir_entry.path());
				}
			}
		}
		else if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".json")
		{
			AddSwap(dir_entry.path());
		}
	}
	std::sort(loaded_swaps.begin(), loaded_swaps.end(), SwapCompare);
}

std::vector<RValue> delete_sprites;

void UnloadSwaps()
{
	for (json swap : loaded_swaps)
	{
		std::string id = swap["id"].get<std::string>();
		if (!swap_sprites.contains(id))
		{
			continue;
		}
		RValue sprite = swap_sprites[id];
		std::string sprite_name = g_ModuleInterface->CallBuiltin("sprite_get_name", {sprite}).ToString();
		if (!sprite_name.starts_with("spr"))
		{
			delete_sprites.push_back(sprite);
		}

		swap_sprites.erase(id);
		swap_loco.erase(id);
	}

	loaded_swaps.clear();
	loaded_swaps.shrink_to_fit();
}

void DeleteSprites()
{
	RValue sprite_beastie_ball_impact = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("sprite_beastie_ball_impact")});
	RValue char_anims = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("char_animations")});
	for (RValue sprite : delete_sprites)
	{
		std::string sprite_name = g_ModuleInterface->CallBuiltin("sprite_get_name", {sprite}).ToString();
		g_ModuleInterface->CallBuiltin("variable_struct_remove", {sprite_beastie_ball_impact, RValue("_" + sprite_name)});
		g_ModuleInterface->CallBuiltin("ds_map_delete", {char_anims, sprite});
		g_ModuleInterface->CallBuiltin("sprite_delete", {sprite});
	}
	delete_sprites.clear();
	delete_sprites.shrink_to_fit();
}

void FrameCallback(FWFrame &FrameContext)
{
	UNREFERENCED_PARAMETER(FrameContext);

	AurieStatus last_status = AURIE_SUCCESS;

	static uint32_t frame_counter = 0;
	static bool reloaded_last_frame = false;

	if (frame_counter > 100)
	{
		if (delete_sprites.size())
		{
			DeleteSprites();
		}
		short ctrl = GetAsyncKeyState(VK_CONTROL);
		short shift = GetAsyncKeyState(VK_SHIFT);
		short r = GetAsyncKeyState('R');
		if (ctrl && shift && r)
		{
			if (!reloaded_last_frame)
			{
				UnloadSwaps();
				LoadSwaps();
				reloaded_last_frame = true;
			}
		}
		else
		{
			reloaded_last_frame = false;
		}

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
