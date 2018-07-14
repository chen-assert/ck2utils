
#include "DefaultMap.h"
#include "parser.h"
#include <unordered_map>
#include <string_view>


_CK2_NAMESPACE_BEGIN;


DefaultMap::DefaultMap(const VFS& vfs)
: _max_prov_id(0)
{
    using ParseError = parser::ParseError;

    const std::unordered_map<const char*, fs::path&> req_path_map = {
        {"definitions", _definitions_path},
        {"provinces", _province_map_path},
        {"positions", _positions_path},
        {"terrain", _terrain_map_path},
        {"rivers", _river_map_path},
        {"terrain_definition", _terrain_path},
        {"heightmap", _height_map_path},
        {"tree_definition", _tree_map_path},
        {"continent", _continent_path},
        {"adjacencies", _adjacencies_path},
        {"climate", _climate_path},
        {"geographical_region", _geo_region_path},
        {"region", _island_region_path},
        {"static", _statics_path},
        {"seasons", _seasons_path},
    };

    auto my_path = vfs["map/default.map"];
    parser parse(my_path);

    for (const auto& s : *parse.root_block())
    {
        if (s.key() == "max_provinces")
        {
            if (!s.value().is_integer())
                throw ParseError(s.value().location(),
                                 "invalid value type for 'max_provinces' (an integer is required)");

            auto max_provinces = s.value().as_integer();
            const auto min_cap = 2;
            const auto max_cap = std::numeric_limits<uint16_t>::max();

            if (max_provinces < min_cap)
                throw ParseError(s.value().location(),
                                 "max_provinces' value ({}) too low (should be at least {})",
                                 max_provinces, min_cap);

            if (max_provinces > max_cap)
                throw ParseError(s.value().location(),
                                 "max_provinces' value ({}) too high (should be at least {})",
                                 max_provinces, max_cap);

            _max_prov_id = max_provinces - 1;
        }
        else if (s.key() == "sea_zones")
        {
            const auto& obj_list = *s.value().as_list();

            if (obj_list.size() != 2)
                throw ParseError(s.key().location(),
                                 "'sea_zones' range has invalid number of elements (needs exactly 2 province IDs)");

            int i = 0, prov_id_range[2];

            for (auto& o : obj_list)
            {
                if (!o.is_integer())
                    throw ParseError(o.location(),
                                     "non-integer found within 'sea_zones' range (may only contain province IDs)");

                auto& prov_id = prov_id_range[i++] = o.as_integer();

                if (!is_valid_province(prov_id))
                    throw ParseError(o.location(), "invalid province ID #{} in 'sea_zones' range", prov_id);
            }

            if (auto [start, end] = prov_id_range; start <= end)
                _seazone_vec.emplace_back( SeaRange{uint(start), uint(end)} );
            else
                throw ParseError(s.key().location(),
                                 "in 'sea_zones' range, start ID #{} is greater than end ID #{}", start, end);
        }
        else if (s.key() == "major_rivers")
        {
            if (!s.value().is_list())
                throw ParseError(s.value().location(),
                                 "invalid value type for 'major_rivers' clause (requires a list of province IDs)");

            const auto& obj_list = *s.value().as_list();

            for (auto& o : obj_list)
            {
                if (!o.is_integer())
                    throw ParseError(o.location(),
                                     "non-integer in 'major_rivers' clause (may only contain province IDs)");

                if (int prov_id = o.as_integer(); is_valid_province(prov_id))
                    _major_river_set.insert(prov_id);
                else
                    throw ParseError(o.location(),
                                     "invalid province ID #{} in 'major_rivers' clause", prov_id);
            }
        }
        else if (s.key().is_string()) {
            if (auto it = req_path_map.find(s.key().as_string()); it != req_path_map.end()) {
                auto&& [k, v] = *it;

                if (!s.value().is_string())
                    throw ParseError(s.value().location(), "invalid value type for '{}' (requires a string)", k);

                v = s.value().as_string();
            }
        }
    }

    // TODO: marshal & validate externals and ocean_region too!

    if (!_max_prov_id) throw Error("{}: 'max_provinces' not defined", my_path.generic_string());
    if (_seazone_vec.empty()) throw Error("{}: no 'sea_zones' defined", my_path.generic_string());

    for (auto&& [k, v] : req_path_map)
        if (v.empty()) throw Error("{}: '{}' not defined!", k, my_path.generic_string());
}


bool DefaultMap::is_water_province(uint prov_id) const noexcept {
    for (const auto& sea_range : _seazone_vec)
        if (prov_id >= sea_range.start && prov_id <= sea_range.end)
            return true;

    return false;
}

_CK2_NAMESPACE_END;