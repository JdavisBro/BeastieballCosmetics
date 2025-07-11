#include <YYToolkit/YYTK_Shared.hpp>

#include "Hooks.h"
#include "ModuleMain.h"

#include <json.hpp>
using json = nlohmann::json;

using namespace Aurie;
using namespace YYTK;

void CreateHook(std::string HookId, std::string FunctionName, PVOID HookFunction, PVOID *Trampoline)
{
  AurieStatus last_status = AURIE_SUCCESS;
  CScript *script = nullptr;
  last_status = GetScript(FunctionName, script);
  if (!AurieSuccess(last_status))
  {
    return;
  }
  last_status = MmCreateHook(
      g_ArSelfModule,
      HookId,
      script->m_Functions->m_ScriptFunction,
      HookFunction,
      Trampoline);
  if (!AurieSuccess(last_status))
  {
    g_ModuleInterface->PrintWarning("Failed to create hook for %s", FunctionName);
    return;
  }
}

json MatchSwaps(std::string BeastieId, std::string BeastieName)
{
  for (auto swap : loaded_swaps)
  {
    json specie = swap["condition"]["specie"];
    if (specie.is_string())
    {
      if (BeastieId != specie.get<std::string>())
      {
        continue;
      }
    }
    json names = swap["condition"]["names"];
    if (!names.is_array())
    {
      return swap;
    }

    for (auto name : names)
    {
      if (!name.is_string())
      {
        continue;
      }
      if (BeastieName == name.get<std::string>())
      {
        return swap;
      }
    }
  }

  return json{};
}

PFUNC_YYGMLScript charAnimationDrawOriginal = nullptr;
RValue &CharAnimationDrawBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  bool update_sprite = false;
  RValue beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("char")});
  if (!beastie.ToBoolean())
  {
    beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("my_data")});
  }
  if (beastie.ToBoolean())
  {
    std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("specie")}).ToString();
    std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("name")}).ToString();

    json swap = MatchSwaps(beastie_id, beastie_name);
    if (!swap.is_null())
    {
      RValue swap_sprite = swap_sprites[swap["id"].get<std::string>()];
      g_ModuleInterface->CallBuiltin("variable_instance_set", {RValue(Self), RValue("sprite_index_swap"), swap_sprite});
      g_ModuleInterface->CallBuiltin("variable_instance_set", {RValue(Self), RValue("sprite_index"), swap_sprite});
      g_ModuleInterface->CallBuiltin("variable_instance_set", {RValue(Self), RValue("animation_beastie_id"), RValue()});
      RValue swap_loc = swap_loco[swap["id"].get<std::string>()];
      *Args[2] = swap_loc;
      numArgs = max(numArgs, 3);
    }
    else
    {
      g_ModuleInterface->CallBuiltin("variable_instance_set", {RValue(Self), RValue("animation_beastie_id"), beastie});
      update_sprite = true;
    }
  }
  charAnimationDrawOriginal(Self, Other, ReturnValue, numArgs, Args);
  if (update_sprite)
  {
    RValue chars = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("char_dic")});
    RValue specie = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("specie")});
    RValue beastie_template = g_ModuleInterface->CallBuiltin("ds_map_find_value", {chars, specie});
    RValue spr = g_ModuleInterface->CallBuiltin("variable_struct_get", {beastie_template, RValue("spr")});
    g_ModuleInterface->CallBuiltin("variable_instance_set", {RValue(Self), RValue("sprite_index"), spr});
  }

  return ReturnValue;
}

PFUNC_YYGMLScript shaderMonsterOriginal = nullptr;
RValue &ShaderMonsterBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("char")});
  if (!beastie.ToBoolean())
  {
    beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("my_data")});
  }
  if (beastie.ToBoolean())
  {
    // std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("name")}).ToString();
    *Args[0] = beastie;
  }
  shaderMonsterOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

static RValue replaceSprite = RValue();

PFUNC_YYGMLScript spriteAltOriginal = nullptr;
RValue &SpriteAlt(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  spriteAltOriginal(Self, Other, ReturnValue, numArgs, Args);
  if (replaceSprite.ToBoolean())
  {
    ReturnValue = replaceSprite;
    return ReturnValue;
  }

  RValue beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Other), RValue("char")});
  if (!beastie.ToBoolean())
  {
    beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Other), RValue("my_data")});
  }
  if (beastie.ToBoolean())
  {
    std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("specie")}).ToString();
    std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("name")}).ToString();

    json swap = MatchSwaps(beastie_id, beastie_name);
    if (!swap.is_null())
    {
      RValue swap_sprite = swap_sprites[swap["id"].get<std::string>()];
      ReturnValue = swap_sprite;
    }
    // g_ModuleInterface->PrintInfo("%s: %s", beastie_name, ReturnValue.ToString());
  }

  return ReturnValue;
}

PFUNC_YYGMLScript charAnimationOriginal = nullptr;
RValue &CharAnimationBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("char")});
  if (!beastie.ToBoolean())
  {
    beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("my_data")});
  }
  if (beastie.ToBoolean())
  {
    std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("specie")}).ToString();
    std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("name")}).ToString();

    json swap = MatchSwaps(beastie_id, beastie_name);
    if (!swap.is_null())
    {
      RValue swap_sprite = swap_sprites[swap["id"].get<std::string>()];
      g_ModuleInterface->CallBuiltin("variable_instance_set", {RValue(Self), RValue("sprite_index"), swap_sprite});
      if (numArgs >= 4)
      {
        *Args[3] = swap_sprite;
      }
    }
  }
  charAnimationOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

PFUNC_YYGMLScript getColorOriginal = nullptr;
RValue &GetColorBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("specie")}).ToString();
  std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("name")}).ToString();
  try
  {
    json swap = MatchSwaps(beastie_id, beastie_name);
    if (!swap.is_null())
    {
      json colorSet{};
      int colorIndex = (*Args[0]).ToInt32();
      RValue mycol = g_ModuleInterface->CallBuiltin("variable_struct_get", {RValue(Self), RValue("color")});
      int colcount = g_ModuleInterface->CallBuiltin("array_length", {mycol}).ToInt32();
      double colorX = g_ModuleInterface->CallBuiltin("array_get", {mycol, RValue(colorIndex % colcount)}).ToDouble();

      if (colorX >= 2 && swap["colors2"].is_array())
      {
        colorSet = swap["colors2"];
      }
      else if (colorX > 1 && swap["shiny"].is_array())
      {
        colorSet = swap["shiny"];
      }
      else if (swap["colors"].is_array())
      {
        colorSet = swap["colors"];
      }
      if (colorSet.is_array() && colorSet.size() > 0)
      {
        json gradient = colorSet[colorIndex % colorSet.size()];
        colorX = colorX - (int)colorX;
        if (gradient.is_object())
        {
          if (gradient["array"].is_array())
          {
            gradient = gradient["array"];
          }
        }
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
            ReturnValue = RValue(colorB["color"].get<double>());
            return ReturnValue;
          }
          else if (!colorA.is_null() && colorB.is_null())
          {
            ReturnValue = RValue(colorA["color"].get<double>());
            return ReturnValue;
          }
          else if (!colorA.is_null() && !colorB.is_null())
          {
            double valueA = colorA["color"].get<double>();
            double xA = colorA["x"].get<double>();
            double valueB = colorB["color"].get<double>();
            double xB = colorB["x"].get<double>();
            ReturnValue = g_ModuleInterface->CallBuiltin("merge_color", {RValue(valueA),
                                                                         RValue(valueB),
                                                                         RValue((colorX - xA) / (xB - xA))});
            return ReturnValue;
          }
        }
      }
    }
  }
  catch (json::exception &e)
  {
    g_ModuleInterface->PrintInfo(e.what());
  }
  getColorOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

static bool setActionFrame = false;

PFUNC_YYGMLScript getColorNumOriginal = nullptr;
RValue &GetColorNumBefore(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("specie")}).ToString();
  std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("name")}).ToString();
  json swap = MatchSwaps(beastie_id, beastie_name);
  if (!swap.is_null())
  {
    if (setActionFrame)
    {
      RValue objActionframe = g_ModuleInterface->CallBuiltin("asset_get_index", {"objActionframe"});
      int actionFrameCount = g_ModuleInterface->CallBuiltin("instance_number", {objActionframe}).ToInt32();
      RValue actionFrame;
      for (int i = 0; i < actionFrameCount; i++)
      {
        actionFrame = g_ModuleInterface->CallBuiltin("instance_find", {objActionframe, RValue(i)});
        if (g_ModuleInterface->CallBuiltin("variable_instance_get", {actionFrame, RValue("time")}).ToInt32() < 0)
        {
          break;
        }
      }
      RValue swap_sprite = swap_sprites[swap["id"].get<std::string>()];
      g_ModuleInterface->CallBuiltin("variable_instance_set", {actionFrame,
                                                               RValue("sprite_index"),
                                                               swap_sprite});
    }

    size_t numCol = swap["colors"].is_array() ? swap["colors"].size() : 0;
    size_t numCol2 = swap["colors2"].is_array() ? swap["colors2"].size() : 0;
    size_t numShiny = swap["shiny"].is_array() ? swap["shiny"].size() : 0;
    size_t maxNum = max(numCol, max(numCol2, numShiny));
    if (maxNum > 0)
    {
      ReturnValue = RValue(maxNum);
      return ReturnValue;
    }
  }
  getColorNumOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

// double GetSpriteScale(RValue sprite)
// {
//   double height = g_ModuleInterface->CallBuiltin("sprite_get_bbox_bottom", {sprite}).ToDouble() - g_ModuleInterface->CallBuiltin("sprite_get_bbox_top", {sprite}).ToDouble();
//   double width = g_ModuleInterface->CallBuiltin("sprite_get_bbox_right", {sprite}).ToDouble() - g_ModuleInterface->CallBuiltin("sprite_get_bbox_left", {sprite}).ToDouble();
//   double yScale = 84 / height;
//   double xScale = 99.75 / width;
//   return min(yScale, xScale);
// }

PFUNC_YYGMLScript drawMonsterMenuOriginal = nullptr;
RValue &DrawMonsterMenu(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = *Args[0];
  std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("specie")}).ToString();
  std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("name")}).ToString();

  json swap = MatchSwaps(beastie_id, beastie_name);
  if (!swap.is_null())
  {
    RValue swap_sprite = swap_sprites[swap["id"].get<std::string>()];
    replaceSprite = swap_sprite;
    // double new_scale = GetSpriteScale(swap_sprite); // atempt to fix menu beasties being too big
    // RValue char_dic = g_ModuleInterface->CallBuiltin("variable_global_get", {RValue("char_dic")});
    // RValue current_beastie = g_ModuleInterface->CallBuiltin("ds_map_find_value", {char_dic, RValue(beastie_id)});
    // double old_scale = GetSpriteScale(g_ModuleInterface->CallBuiltin("variable_instance_get", {current_beastie, RValue("spr")}));
    // double scale = (*Args[3]).ToDouble();
    // *Args[3] = RValue(scale * (new_scale / old_scale));
  }
  drawMonsterMenuOriginal(Self, Other, ReturnValue, numArgs, Args);
  replaceSprite = RValue();
  return ReturnValue;
}

PFUNC_YYGMLScript spriteOriginal = nullptr;
RValue &Sprite(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  spriteOriginal(Self, Other, ReturnValue, numArgs, Args);
  if (replaceSprite.ToBoolean())
  {
    ReturnValue = replaceSprite;
    return ReturnValue;
  }

  RValue beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Other), RValue("char")});
  if (!beastie.ToBoolean())
  {
    beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Other), RValue("my_data")});
  }
  if (beastie.ToBoolean())
  {
    std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("specie")}).ToString();
    std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("name")}).ToString();

    json swap = MatchSwaps(beastie_id, beastie_name);
    if (!swap.is_null())
    {
      RValue swap_sprite = swap_sprites[swap["id"].get<std::string>()];
      ReturnValue = swap_sprite;
    }
    // g_ModuleInterface->PrintInfo("%s: %s", beastie_name, ReturnValue.ToString());
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
  bool update_sprite = false;
  RValue beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("char")});
  if (!beastie.ToBoolean())
  {
    beastie = g_ModuleInterface->CallBuiltin("variable_instance_get", {RValue(Self), RValue("my_data")});
  }
  if (beastie.ToBoolean())
  {
    std::string beastie_id = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("specie")}).ToString();
    std::string beastie_name = g_ModuleInterface->CallBuiltin("variable_instance_get", {beastie, RValue("name")}).ToString();

    json swap = MatchSwaps(beastie_id, beastie_name);
    if (!swap.is_null())
    {
      RValue swap_loc = swap_loco[swap["id"].get<std::string>()];
      *Args[0] = swap_loc;
    }
  }
  locomoteOriginal(Self, Other, ReturnValue, numArgs, Args);

  return ReturnValue;
}

void CreateAllHooks()
{
  CreateHook("BC CharAnimDraw", "gml_Script_char_animation_draw", CharAnimationDrawBefore, reinterpret_cast<PVOID *>(&charAnimationDrawOriginal));
  CreateHook("BC ShaderMonster", "gml_Script_shader_monster", ShaderMonsterBefore, reinterpret_cast<PVOID *>(&shaderMonsterOriginal));
  CreateHook("BC SpriteAlt", "gml_Script_sprite_alt@anon@14908@class_beastie_template@class_beastie_template", SpriteAlt, reinterpret_cast<PVOID *>(&spriteAltOriginal));
  CreateHook("BC CharAnim", "gml_Script_char_animation", CharAnimationBefore, reinterpret_cast<PVOID *>(&charAnimationOriginal));
  CreateHook("BC GetColor", "gml_Script_get_color@anon@33539@class_beastie_global@class_beastie", GetColorBefore, reinterpret_cast<PVOID *>(&getColorOriginal));
  CreateHook("BC GetColorNum", "gml_Script_get_color_num@anon@32936@class_beastie_global@class_beastie", GetColorNumBefore, reinterpret_cast<PVOID *>(&getColorNumOriginal));

  CreateHook("BC DrawMonsterMenu", "gml_Script_draw_monster_menu", DrawMonsterMenu, reinterpret_cast<PVOID *>(&drawMonsterMenuOriginal));
  CreateHook("BC Sprite", "gml_Script_sprite@anon@14851@class_beastie_template@class_beastie_template", Sprite, reinterpret_cast<PVOID *>(&spriteOriginal));

  CreateHook("BC ActionFrame", "gml_Script_ActionFrame", ActionFrame, reinterpret_cast<PVOID *>(&actionFrameOriginal));

  CreateHook("BC Locomote", "gml_Script_locomote", LocomoteBefore, reinterpret_cast<PVOID *>(&locomoteOriginal));
}
