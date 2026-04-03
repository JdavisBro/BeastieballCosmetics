#include <YYToolkit/YYTK_Shared.hpp>

#include "Hooks.h"
#include "ModuleMain.h"

#include <json.hpp>
using json = nlohmann::json;

using namespace Aurie;
using namespace YYTK;

struct NameCheck
{
  const char *pre;
  const char *post;
};

std::map<std::string, CScript *> found_scripts;

void FindScripts()
{
  std::vector<NameCheck> lost_scripts = {
    {NULL, "gml_Script_char_animation_draw"},
    {NULL, "gml_Script_shader_monster"},
    {"gml_Script_sprite_alt@", "class_beastie_template"},
    {NULL, "gml_Script_char_animation"},
    {"gml_Script_get_color@", "class_beastie"},
    {"gml_Script_get_color_num@", "class_beastie"},
    {NULL, "gml_Script_draw_monster_menu"},
    {"gml_Script_sprite@", "class_beastie_template"},
    {NULL, "gml_Script_ActionFrame"},
    {NULL, "gml_Script_locomote"},
  };

  AurieStatus last_status = AURIE_SUCCESS;
  CScript *script = nullptr;
  int i = 0;
  while (true)
  {
    last_status = yytk->GetScriptData(i, script);
    if (last_status != AURIE_SUCCESS)
    {
      if (last_status != AURIE_OBJECT_NOT_FOUND)
      {
        DbgPrintEx(LOG_SEVERITY_WARNING, AurieStatusToString(last_status));
      }
      break;
    }
    std::string script_name = script->GetName();
    for (size_t lost_i = 0; lost_i < lost_scripts.size(); lost_i++)
    {
      NameCheck name_check = lost_scripts[lost_i];
      if (script_name.ends_with(name_check.post) && (name_check.pre == NULL || script_name.starts_with(name_check.pre)))
      {
        std::string combined_name = std::string(name_check.pre == NULL ? "" : name_check.pre) + name_check.post;
        found_scripts[combined_name] = script;
        lost_scripts.erase(lost_scripts.begin() + lost_i);
        break;
      }
    }
    i++;
  }
  if (!lost_scripts.empty())
  {
    for (NameCheck name_check : lost_scripts)
    {
      DbgPrintEx(LOG_SEVERITY_WARNING, "Unable to find script for %s%s", name_check.pre == NULL ? "" : name_check.pre, name_check.post);
    }
  }
}

bool hook_failed = false;

void CreateHook(std::string HookId, std::string FunctionName, PVOID HookFunction, PVOID *Trampoline)
{
  AurieStatus last_status = AURIE_SUCCESS;
  if (!found_scripts.contains(FunctionName))
  {
    hook_failed = true;
    return;
  }
  CScript *script = found_scripts[FunctionName];
  last_status = MmCreateHook(
    g_ArSelfModule,
    HookId,
    script->m_Functions->m_ScriptFunction,
    HookFunction,
    Trampoline
  );
  if (!AurieSuccess(last_status))
  {
    hook_failed = true;
    DbgPrintEx(LOG_SEVERITY_WARNING, "Failed to create hook for %s", FunctionName);
    return;
  }
}

BeastieSwap* MatchSwaps(std::string &BeastieId, std::string &BeastieName, bool MustHaveSprite)
{
  for (BeastieSwap &swap : loaded_beastie_swaps)
  {
    if (MustHaveSprite && !swap.has_sprite)
      continue;
    if (swap.needs_specie && BeastieId != swap.specie)
      continue;
    if (!swap.needs_names)
      return &swap;
    for (std::string &name : swap.names)
    {
      if (BeastieName == name)
        return &swap;
    }
  }
  return nullptr;
}

RValue GetBeastie(CInstance *Instance)
{
  RValue RV_Inst = RValue(Instance);
  RValue beastie;
  yytk->CallBuiltinEx(beastie, "variable_instance_get", nullptr, nullptr, {RV_Inst, "char"});
  if (beastie.IsUndefined())
    yytk->CallBuiltinEx(beastie, "variable_instance_get", nullptr, nullptr, {RV_Inst, "my_data"});
  return beastie;
}

PFUNC_YYGMLScript charAnimationDrawOriginal = nullptr;
RValue &CharAnimationDrawBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  bool update_sprite = false;
  RValue beastie = GetBeastie(Self);
  RValue RV_Self = RValue(Self);
  if (!beastie.IsUndefined())
  {
    std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
    std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();

    BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, true);
    if (swap)
    {
      RValue swap_sprite = swap->sprite;
      yytk->CallBuiltin("variable_instance_set", {RV_Self, "sprite_index_swap", swap_sprite});
      yytk->CallBuiltin("variable_instance_set", {RV_Self, "sprite_index", swap_sprite});
      yytk->CallBuiltin("variable_instance_set", {RV_Self, "animation_beastie_id", RValue()});
      *Args[2] = swap->loco;
      numArgs = max(numArgs, 3);
    }
    else
    {
      yytk->CallBuiltin("variable_instance_set", {RV_Self, "animation_beastie_id", beastie});
      update_sprite = true;
    }
  }
  charAnimationDrawOriginal(Self, Other, ReturnValue, numArgs, Args);
  if (update_sprite)
  {
    RValue chars = yytk->CallBuiltin("variable_global_get", {"char_dic"});
    RValue specie = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"});
    RValue beastie_template = yytk->CallBuiltin("ds_map_find_value", {chars, specie});
    RValue spr = yytk->CallBuiltin("variable_struct_get", {beastie_template, "spr"});
    yytk->CallBuiltin("variable_instance_set", {RV_Self, "sprite_index", spr});
  }

  return ReturnValue;
}

PFUNC_YYGMLScript shaderMonsterOriginal = nullptr;
RValue &ShaderMonsterBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = GetBeastie(Self);
  if (!beastie.IsUndefined())
    *Args[0] = beastie;
  beastie = *Args[0];
  if (yytk->CallBuiltin("instanceof", {beastie}).ToString() == "class_beastie_template") // For shader monster calls with a template, make a beastie so colors are correct
    *Args[0] = yytk->CallBuiltin("method_call", {
      yytk->CallBuiltin("method", {beastie, beastie["generate"]}),
      RValue(std::vector<RValue>{5, 0})
    });
  shaderMonsterOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

static RValue *replaceSprite = nullptr;

PFUNC_YYGMLScript spriteAltOriginal = nullptr;
RValue &SpriteAlt(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  spriteAltOriginal(Self, Other, ReturnValue, numArgs, Args);
  if (replaceSprite && replaceSprite->ToBoolean())
  {
    ReturnValue = *replaceSprite;
    return ReturnValue;
  }

  RValue beastie = GetBeastie(Other);
  if (!beastie.IsUndefined())
  {
    std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
    std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();

    BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, true);
    if (swap)
      ReturnValue = swap->sprite;
    // DbgPrintEx(LOG_SEVERITY_INFO,"%s: %s", beastie_name, ReturnValue.ToString());
  }

  return ReturnValue;
}

PFUNC_YYGMLScript charAnimationOriginal = nullptr;
RValue &CharAnimationBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = GetBeastie(Self);
  if (!beastie.IsUndefined())
  {
    std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
    std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();

    BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, true);
    if (swap)
    {
      RValue &swap_sprite = swap->sprite;
      yytk->CallBuiltin("variable_instance_set", {RValue(Self), "sprite_index", swap_sprite});
      if (numArgs >= 4)
        *Args[3] = swap_sprite;
    }
  }
  charAnimationOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

PFUNC_YYGMLScript getColorOriginal = nullptr;
RValue &GetColorBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = RValue(Self);
  std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
  std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();
  try
  {
    BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, false);
    if (swap && !swap->colors.is_null())
    {
      json colorSet{};
      int colorIndex = (*Args[0]).ToInt32();
      RValue mycol = yytk->CallBuiltin("variable_struct_get", {beastie, "color"});
      int colcount = yytk->CallBuiltin("array_length", {mycol}).ToInt32();
      double colorX = yytk->CallBuiltin("array_get", {mycol, colorIndex % colcount}).ToDouble();

      json &swap_colors = swap->colors;
      bool colors2exists = swap_colors["colors2"].is_array();
      bool shinyexists = swap_colors["shiny"].is_array();
      bool colorsexists = swap_colors["colors"].is_array();
      if ((colorX >= 2 || (!shinyexists && !colorsexists)) && colors2exists)
        colorSet = swap_colors["colors2"];
      else if ((colorX > 1 || !colorsexists) && shinyexists)
        colorSet = swap_colors["shiny"];
      else if (colorsexists)
        colorSet = swap_colors["colors"];
      if (colorSet.is_array() && colorSet.size() > 0)
      {
        json gradient = colorSet[colorIndex % colorSet.size()];
        colorX = colorX - (int)colorX;
        if (gradient.is_object())
          if (gradient["array"].is_array())
            gradient = gradient["array"];
        if (gradient.is_array())
        {
          json colorA{};
          json colorB{};
          for (auto col : gradient)
          {
            double currentX = col["x"].get<double>();
            if (colorX == currentX)
            {
              colorA = col;
              colorB = json{};
              break;
            }
            if (colorX > currentX)
            {
              colorA = col;
              continue;
            }
            if (colorX < currentX)
            {
              colorB = col;
              break;
            }
          }
          if (colorA.is_null() && !colorB.is_null())
          {
            ReturnValue = colorB["color"].get<double>();
            return ReturnValue;
          }
          else if (!colorA.is_null() && colorB.is_null())
          {
            ReturnValue = colorA["color"].get<double>();
            return ReturnValue;
          }
          else if (!colorA.is_null() && !colorB.is_null())
          {
            double valueA = colorA["color"].get<double>();
            double xA = colorA["x"].get<double>();
            double valueB = colorB["color"].get<double>();
            double xB = colorB["x"].get<double>();
            ReturnValue = yytk->CallBuiltin("merge_color", {valueA, valueB, (colorX - xA) / (xB - xA)});
            return ReturnValue;
          }
        }
      }
    }
  }
  catch (json::exception &e)
  {
    DbgPrintEx(LOG_SEVERITY_INFO, e.what());
  }
  getColorOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

static bool setActionFrame = false;

PFUNC_YYGMLScript getColorNumOriginal = nullptr;
RValue &GetColorNumBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = RValue(Self);
  std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
  std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();
  BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, false);
  getColorNumOriginal(Self, Other, ReturnValue, numArgs, Args);
  size_t gameCount = ReturnValue.ToInt32();
  if (swap)
  {
    // this is called directly after the actionframe's sprite_index is set, i overwrite it here only during an ActionFrame call.
    if (setActionFrame && swap->has_sprite)
    {
      RValue objActionframe = yytk->CallBuiltin("asset_get_index", {"objActionframe"});
      int actionFrameCount = yytk->CallBuiltin("instance_number", {objActionframe}).ToInt32();
      RValue actionFrame;
      for (int i = 0; i < actionFrameCount; i++)
      {
        actionFrame = yytk->CallBuiltin("instance_find", {objActionframe, i});
        if (yytk->CallBuiltin("variable_instance_get", {actionFrame, "time"}).ToInt32() < 0) // new ActionFrame have time = -1
          break;
      }
      yytk->CallBuiltin("variable_instance_set", {actionFrame, "sprite_index", swap->sprite});
    }

    json &swap_colors = swap->colors;
    size_t numCol = swap_colors["colors"].is_array() ? swap_colors["colors"].size() : 0;
    size_t numCol2 = swap_colors["colors2"].is_array() ? swap_colors["colors2"].size() : 0;
    size_t numShiny = swap_colors["shiny"].is_array() ? swap_colors["shiny"].size() : 0;
    size_t maxNum = max(numCol, max(numCol2, numShiny));
    if (maxNum > 0 && maxNum > gameCount)
      ReturnValue = maxNum;
  }

  return ReturnValue;
}

// double GetSpriteScale(RValue sprite)
// {
//   double height = yytk->CallBuiltin("sprite_get_bbox_bottom", {sprite}).ToDouble() - yytk->CallBuiltin("sprite_get_bbox_top", {sprite}).ToDouble();
//   double width = yytk->CallBuiltin("sprite_get_bbox_right", {sprite}).ToDouble() - yytk->CallBuiltin("sprite_get_bbox_left", {sprite}).ToDouble();
//   double yScale = 84 / height;
//   double xScale = 99.75 / width;
//   return min(yScale, xScale);
// }

PFUNC_YYGMLScript drawMonsterMenuOriginal = nullptr;
RValue &DrawMonsterMenu(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = *Args[0];
  std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
  std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();

  BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, true);
  if (swap)
  {
    replaceSprite = &swap->sprite;
    // double new_scale = GetSpriteScale(swap_sprite); // atempt to fix menu beasties being too big
    // RValue char_dic = yytk->CallBuiltin("variable_global_get", {"char_dic"});
    // RValue current_beastie = yytk->CallBuiltin("ds_map_find_value", {char_dic, RValue(beastie_id)});
    // double old_scale = GetSpriteScale(yytk->CallBuiltin("variable_instance_get", {current_beastie, "spr"}));
    // double scale = (*Args[3]).ToDouble();
    // *Args[3] = scale * (new_scale / old_scale);
  }
  drawMonsterMenuOriginal(Self, Other, ReturnValue, numArgs, Args);
  replaceSprite = nullptr;
  return ReturnValue;
}

PFUNC_YYGMLScript spriteOriginal = nullptr;
RValue &Sprite(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  spriteOriginal(Self, Other, ReturnValue, numArgs, Args);
  if (replaceSprite && replaceSprite->ToBoolean())
  {
    ReturnValue = *replaceSprite;
    return ReturnValue;
  }

  RValue beastie = GetBeastie(Other);
  if (!beastie.IsUndefined())
  {
    std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
    std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();

    BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, true);
    if (swap)
      ReturnValue = swap->sprite;
    // DbgPrintEx(LOG_SEVERITY_INFO,"%s: %s", beastie_name, ReturnValue.ToString());
  }

  return ReturnValue;
}

PFUNC_YYGMLScript actionFrameOriginal = nullptr;
RValue &ActionFrame(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  setActionFrame = true;
  actionFrameOriginal(Self, Other, ReturnValue, numArgs, Args);
  setActionFrame = false;
  return ReturnValue;
}

PFUNC_YYGMLScript locomoteOriginal = nullptr;
RValue &LocomoteBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = GetBeastie(Self);
  if (!beastie.IsUndefined())
  {
    std::string beastie_id = yytk->CallBuiltin("variable_instance_get", {beastie, "specie"}).ToString();
    std::string beastie_name = yytk->CallBuiltin("variable_instance_get", {beastie, "name"}).ToString();

    BeastieSwap *swap = MatchSwaps(beastie_id, beastie_name, true);
    if (swap)
      *Args[0] = swap->loco;
  }
  locomoteOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

void CreateAllHooks()
{
  FindScripts();

  CreateHook("BC CharAnimDraw", "gml_Script_char_animation_draw", CharAnimationDrawBefore, reinterpret_cast<PVOID *>(&charAnimationDrawOriginal));
  CreateHook("BC ShaderMonster", "gml_Script_shader_monster", ShaderMonsterBefore, reinterpret_cast<PVOID *>(&shaderMonsterOriginal));
  CreateHook("BC SpriteAlt", "gml_Script_sprite_alt@class_beastie_template", SpriteAlt, reinterpret_cast<PVOID *>(&spriteAltOriginal));
  CreateHook("BC CharAnim", "gml_Script_char_animation", CharAnimationBefore, reinterpret_cast<PVOID *>(&charAnimationOriginal));
  CreateHook("BC GetColor", "gml_Script_get_color@class_beastie", GetColorBefore, reinterpret_cast<PVOID *>(&getColorOriginal));
  CreateHook("BC GetColorNum", "gml_Script_get_color_num@class_beastie", GetColorNumBefore, reinterpret_cast<PVOID *>(&getColorNumOriginal));

  CreateHook("BC DrawMonsterMenu", "gml_Script_draw_monster_menu", DrawMonsterMenu, reinterpret_cast<PVOID *>(&drawMonsterMenuOriginal));
  CreateHook("BC Sprite", "gml_Script_sprite@class_beastie_template", Sprite, reinterpret_cast<PVOID *>(&spriteOriginal));

  CreateHook("BC ActionFrame", "gml_Script_ActionFrame", ActionFrame, reinterpret_cast<PVOID *>(&actionFrameOriginal));

  CreateHook("BC Locomote", "gml_Script_locomote", LocomoteBefore, reinterpret_cast<PVOID *>(&locomoteOriginal));

  if (hook_failed)
  {
    DbgPrintEx(LOG_SEVERITY_ERROR, "[BeastieballCosmetics] - A Hook Failed...");
  }
  else
  {
    DbgPrintEx(LOG_SEVERITY_DEBUG, "[BeastieballCosmetics] - All Hooks are Successful!");
  }
}
