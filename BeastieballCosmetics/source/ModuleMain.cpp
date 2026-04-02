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

YYTKInterface *yytk = nullptr;

AurieStatus GetScript(const char *FunctionName, CScript *&script)
{
	AurieStatus last_status = AURIE_SUCCESS;
	int index;
	last_status = yytk->GetNamedRoutineIndex(FunctionName, &index);
	if (!AurieSuccess(last_status))
	{
		DbgPrintEx(LOG_SEVERITY_WARNING, "Failed to get index for %s", FunctionName);
		return last_status;
	}
	last_status = yytk->GetScriptData(index - 100000, script);
	if (!AurieSuccess(last_status))
	{
		DbgPrintEx(LOG_SEVERITY_WARNING, "Failed to get data for %s", FunctionName);
	}
	return last_status;
}

PFUNC_YYGMLScript elephantFromJson = nullptr;

std::vector<BeastieSwap> loaded_beastie_swaps;

std::string required_keys[] = {"type", "id"};
std::string color_keys[] = {"colors", "shiny", "colors2"};

bool IsColorsValid(json &colors)
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

bool AddSprite(json &spr_data, BeastieSwap &swap)
{
	DbgPrintEx(LOG_SEVERITY_INFO, "[BeastieballCosmetics] - Loading Sprites for %s...", swap.id.c_str());
	if (!spr_data["filename"].is_string())
		return false;
	std::string filename_template = spr_data["filename"].get<std::string>();
	if (!spr_data["count"].is_number())
		return false;
	int file_count = spr_data["count"].get<int>();
	int originX = 484;
	int originY = 989;
	if (spr_data["origin"].is_array() && spr_data["origin"][0].is_number() && spr_data["origin"][1].is_number())
	{
		originX = spr_data["origin"][0].get<int>();
		originY = spr_data["origin"][1].get<int>();
	}
	bool strip = spr_data["strip"].is_boolean() ? spr_data["strip"].get<bool>() : false;

	RValue working_rvalue;
	yytk->GetBuiltin("program_directory", nullptr, NULL_INDEX, working_rvalue);
	std::string working = working_rvalue.ToString();

	std::string path = working + "mod_data/BeastieballCosmetics/" + std::vformat(filename_template, std::make_format_args("0"));
	if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
	{
		DbgPrintEx(LOG_SEVERITY_INFO, "Error loading Sprite %s, file not found %s", swap.id.c_str(), path.c_str());
		return false;
	}
	RValue sprite = yytk->CallBuiltin("sprite_add", {
		RValue(path),
		strip ? file_count : 1,
		false,
		false,
		originX,
		originY,
	});

	std::string spriteStr = yytk->CallBuiltin("sprite_get_name", {sprite}).ToString();

	if (!strip)
	{
		for (int i = 1; i < file_count; i++)
		{
			path = working + "mod_data/BeastieballCosmetics/" + std::vformat(filename_template, std::make_format_args(i));
			if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
			{
				DbgPrintEx(LOG_SEVERITY_INFO, "Error loading Sprite %s, file not found %s", swap.id.c_str(), path.c_str());
				yytk->CallBuiltin("sprite_delete", {sprite});
				return false;
			}
			RValue frame = yytk->CallBuiltin("sprite_add", {
				RValue(path),
				1,
				false,
				false,
				originX,
				originY,
			});
			yytk->CallBuiltin("sprite_merge", {sprite, frame});
			yytk->CallBuiltin("sprite_delete", {frame});
		}
	}

	DbgPrintEx(LOG_SEVERITY_INFO, "[BeastieballCosmetics] - Loaded Sprites for %s!", swap.id.c_str());

	json json_animations = spr_data["animations"];

	RValue sprite_beastie_ball_impact = yytk->CallBuiltin("variable_global_get", {"sprite_beastie_ball_impact"});
	RValue char_anims = yytk->CallBuiltin("variable_global_get", {"char_animations"});

	RValue impact;
	if (json_animations.is_string())
	{
		std::string oldSpriteStr = json_animations.get<std::string>();
		impact = yytk->CallBuiltin("variable_struct_get", {sprite_beastie_ball_impact, RValue("_" + oldSpriteStr)});
	}
	else if (json_animations.is_object())
	{
		RValue parsed = yytk->CallBuiltin("json_parse", {RValue(json_animations.dump())});
		RValue *args[] = {&parsed};
		elephantFromJson(nullptr, nullptr, impact, 1, args);
	}

	if (!impact.ToBoolean())
	{
		DbgPrintEx(LOG_SEVERITY_INFO, "Error Loading Sprite Animations. %s", swap.id.c_str());
		yytk->CallBuiltin("sprite_delete", {sprite});
		return false;
	}
	RValue animations = yytk->CallBuiltin("variable_struct_get", {impact, "anim_data"});
	RValue loco = yytk->CallBuiltin("variable_struct_get", {impact, "loco_data"});
	if (!animations.ToBoolean() || !loco.ToBoolean())
	{
		DbgPrintEx(LOG_SEVERITY_INFO, "Error Loading Sprite Animations. %s", swap.id.c_str());
		yytk->CallBuiltin("sprite_delete", {sprite});
		return false;
	}
	yytk->CallBuiltin("variable_struct_set", {sprite_beastie_ball_impact, RValue("_" + spriteStr), impact});
	yytk->CallBuiltin("ds_map_set", {char_anims, sprite, animations});
	swap.loco = loco;
	swap.sprite = sprite;
	swap.has_sprite = true;
	return true;
}

void AddSwap(const std::filesystem::path &path)
{
	std::string fileName = path.stem().string();
	std::fstream f(path);
	json data = json::parse(f);
	for (std::string key : required_keys)
	{
		if (!data.contains(key))
		{
			DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s - Missing Key %s", fileName.c_str(), key.c_str());
			return;
		}
	}
	if (!data["id"].is_string())
	{
		DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s", fileName.c_str());
		return;
	}
	std::string id = data["id"].get<std::string>();
	DbgPrintEx(LOG_SEVERITY_INFO, "[BeastieballCosmetics] - Loading %s...", id.c_str());

	BeastieSwap swap = {id, false, false};
	swap.id = id;

	json condition = data["condition"];
	if (condition.is_object()) {
		if (condition["specie"].is_string()) {
			swap.needs_specie = true;
			swap.specie = condition["specie"].get<std::string>();
		}
		json names = condition["names"];
		if (names.is_array()) {
			swap.needs_names = true;
			for (size_t i = 0; i < names.size(); i++)
				if (names[i].is_string())
					swap.names.push_back(names[i].get<std::string>());
		}
	}

	json sprite = data["sprite"];
	if (sprite.is_null()) {
		swap.has_sprite = false;
	}
	else if (sprite.is_string()) {
		std::string sprName = sprite.get<std::string>();
		RValue spriteRef = yytk->CallBuiltin("asset_get_index", {RValue(sprName)});
		if (!spriteRef.ToBoolean())
		{
			DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s - Invalid Sprite Name %s", fileName.c_str(), sprName.c_str());
			return;
		}

		RValue sprite_beastie_ball_impact = yytk->CallBuiltin("variable_global_get", {"sprite_beastie_ball_impact"});
		RValue impact = yytk->CallBuiltin("variable_struct_get", {sprite_beastie_ball_impact, RValue("_" + sprName)});
		if (!impact.ToBoolean())
		{
			DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s - Sprite %s has no Beastie anims", fileName.c_str(), sprName.c_str());
			return;
		}
		RValue loco = yytk->CallBuiltin("variable_struct_get", {impact, "loco_data"});
		RValue char_anims = yytk->CallBuiltin("variable_global_get", {"char_animations"});
		RValue char_anims_sprite = yytk->CallBuiltin("ds_map_get", {char_anims, spriteRef});
		if (!char_anims_sprite.ToBoolean()) // some beastie sprites (alt sprites like sprRat_alt) aren't in char_animations
		{
			RValue animations = yytk->CallBuiltin("variable_struct_get", {impact, "anim_data"});
			yytk->CallBuiltin("ds_map_set", {char_anims, spriteRef, animations});
		}

		swap.sprite = spriteRef;
		swap.loco = loco;
		swap.has_sprite = true;
	}
	else {
		if (!sprite.is_object()) {
			DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s - Invalid Sprite", fileName.c_str());
			return;
		}
		if (!AddSprite(sprite, swap)) {
			DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s - Invalid Sprite", fileName.c_str());
			return;
		}
	}

	RValue char_dic = yytk->CallBuiltin("variable_global_get", {"char_dic"});
	json colors = json::object({});
	bool any_colors = false;
	for (std::string key : color_keys)
	{
		if (!data.contains(key))
			continue;
		json color = data[key];
		if (color.is_string()) {
			RValue beastie = yytk->CallBuiltin("ds_map_find_value", {char_dic, RValue(color.get<std::string>())});
			if (beastie.ToBoolean()) {
				RValue coldata = yytk->CallBuiltin("variable_instance_get", {beastie, RValue(key)});
				colors[key] = json::parse(yytk->CallBuiltin("json_stringify", {coldata}).ToString());
				any_colors = true;
			}
			else {
				DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s - %s has beastie %s which doesn't exist", fileName.c_str(), key.c_str(), color.get<std::string>().c_str());
			}
		}
		else if (IsColorsValid(data[key])) {
			colors[key] = color;
			any_colors = true;
		}
		else {
			DbgPrintEx(LOG_SEVERITY_WARNING, "Error Loading %s - %s invalid format.", fileName.c_str(), key.c_str());
		}
	}
	swap.colors = any_colors ? colors : json{};
	DbgPrintEx(LOG_SEVERITY_INFO, "[BeastieballCosmetics] - Loaded %s", id.c_str());
	loaded_beastie_swaps.push_back(swap);
}

inline size_t GetConditionValue(const BeastieSwap &swap)
{
	return (size_t)swap.needs_specie + (size_t)swap.needs_names + swap.names.size();
}

bool SwapCompare(const BeastieSwap &swap1, const BeastieSwap &swap2)
{
	// should prioritise swaps with more conditions to not be overwritten by swaps with less conditions.
	size_t cond1 = GetConditionValue(swap1);
	size_t cond2 = GetConditionValue(swap2);
	return cond1 != cond2 ? cond1 > cond2 : swap1.id.compare(swap2.id) >= 0;
}

void LoadSwaps()
{
	DbgPrintEx(LOG_SEVERITY_DEBUG, "[BeastieballCosmetics] - Loading Swaps!");

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
	std::sort(loaded_beastie_swaps.begin(), loaded_beastie_swaps.end(), SwapCompare);
	DbgPrintEx(LOG_SEVERITY_DEBUG, "[BeastieballCosmetics] - Finished Loading Swaps!");
}

std::vector<RValue> delete_sprites;

void UnloadSwaps()
{
	for (BeastieSwap &swap : loaded_beastie_swaps)
	{
		std::string sprite_name = yytk->CallBuiltin("sprite_get_name", {swap.sprite}).ToString();
		if (!sprite_name.starts_with("spr"))
			delete_sprites.push_back(swap.sprite);
	}

	loaded_beastie_swaps.clear();
}

void DeleteSprites()
{
	RValue sprite_beastie_ball_impact = yytk->CallBuiltin("variable_global_get", {"sprite_beastie_ball_impact"});
	RValue char_anims = yytk->CallBuiltin("variable_global_get", {"char_animations"});
	for (RValue sprite : delete_sprites)
	{
		std::string sprite_name = yytk->CallBuiltin("sprite_get_name", {sprite}).ToString();
		yytk->CallBuiltin("variable_struct_remove", {sprite_beastie_ball_impact, RValue("_" + sprite_name)});
		yytk->CallBuiltin("ds_map_delete", {char_anims, sprite});
		yytk->CallBuiltin("sprite_delete", {sprite});
	}
	delete_sprites.clear();
	delete_sprites.shrink_to_fit();
}

void CodeCallback(FWCodeEvent &Event)
{
	auto [Self, Other, Code, ArgCount, Arg] = Event.Arguments();

	std::string name = Code->GetName();

	if (name != "gml_Object_objGame_Draw_0")
		return;

	AurieStatus last_status = AURIE_SUCCESS;

	static uint32_t frame_counter = 0;
	static bool reloaded_last_frame = false;

	if (frame_counter > 100)
	{
		if (delete_sprites.size())
		{
			DeleteSprites();
		}
		bool ctrl = yytk->CallBuiltin("keyboard_check", {17.0}).ToBoolean();
		bool shift = yytk->CallBuiltin("keyboard_check", {16.0}).ToBoolean();
		bool r = yytk->CallBuiltin("keyboard_check", {82.0}).ToBoolean();
		if (ctrl && shift && r)
		{
			if (!reloaded_last_frame)
			{
				UnloadSwaps();
				LoadSwaps();
				loaded_beastie_swaps.shrink_to_fit();
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
			DbgPrintEx(LOG_SEVERITY_WARNING, "eror!! %s", e.what());
		}
	}
	frame_counter++;
}

EXPORTED AurieStatus ModuleInitialize(
	IN AurieModule *Module,
	IN const fs::path &ModulePath)
{
	UNREFERENCED_PARAMETER(ModulePath);

	AurieStatus last_status = AURIE_SUCCESS;

	// Gets a handle to the interface exposed by YYTK
	// You can keep this pointer for future use, as it will not change unless YYTK is unloaded.
	yytk = YYTK::GetInterface();

	// If we can't get the interface, we fail loading.
	if (!yytk)
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

	CreateAllHooks();

	last_status = yytk->CreateCallback(
		Module,
		EVENT_OBJECT_CALL,
		CodeCallback,
		0);

	if (!AurieSuccess(last_status))
	{
		DbgPrintEx(LOG_SEVERITY_ERROR, "[BeastieballCosmetics] - Failed to register frame callback!");
	}

	DbgPrintEx(LOG_SEVERITY_DEBUG, "[BeastieballCosmetics] - Hello from PluginEntry!");

	return AURIE_SUCCESS;
}
