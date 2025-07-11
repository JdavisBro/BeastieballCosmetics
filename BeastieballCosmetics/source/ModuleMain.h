#pragma once
#include <YYToolkit/YYTK_Shared.hpp>

#include <json.hpp>
using json = nlohmann::json;

using namespace Aurie;
using namespace YYTK;

extern YYTKInterface *g_ModuleInterface;
extern std::vector<json> loaded_swaps;
extern std::map<std::string, RValue> swap_sprites;
extern std::map<std::string, RValue> swap_loco;
