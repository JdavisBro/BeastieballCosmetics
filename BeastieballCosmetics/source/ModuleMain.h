#pragma once
#include <YYToolkit/YYTK_Shared.hpp>

#include <json.hpp>
using json = nlohmann::json;

using namespace Aurie;
using namespace YYTK;

extern YYTKInterface *yytk;

struct BeastieSwap {
	std::string id;
	bool needs_specie;
	bool needs_names;
	std::string specie;
	std::vector<std::string> names;
    bool has_sprite;
	RValue sprite;
	RValue loco;
    json colors;
};

extern std::vector<BeastieSwap> loaded_beastie_swaps;
