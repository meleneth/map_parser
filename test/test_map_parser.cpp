#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <string_view>

#include "map_parser.hpp"

using namespace fallout2::maps;

namespace {

constexpr std::string_view kSample = R"(; Map datafile for worldmap.c, etc.
; Fallout 2

[Map 000]
lookup_name=Desert Encounter 1
map_name=desert1
music=07desert
ambient_sfx=gustwind:20, gustwin1:20, rattle:15, rattle1:15, vulture:15, vulture1:15
saved=No  ; Random encounter maps aren't saved normally
dead_bodies_age=No
can_rest_here=No,No,No  ; All 3 elevations
random_start_point_0=elev:0, tile_num:19086
random_start_point_1=elev:0, tile_num:17302
;random_start_point_9=elev:9, tile_num:99999

[Map 001]
lookup_name=Arroyo Caves
map_name=arcaves
music=13CARVRN
ambient_sfx=water:40, water1:25, animal:15, animal1:10, pebble:5, pebble1:5
pipboy_active=No
saved=Yes
can_rest_here=No,No,No
custom_field=kept-as-raw
)";

constexpr std::string_view kBadMusicLine = R"([Map 000]
lookup_name=Wanamingo Mine Entrance
map_name=redwame
music-24redd
saved=Yes
)";

constexpr std::string_view kGapInIds = R"([Map 000]
lookup_name=One
map_name=one

[Map 002]
lookup_name=Three
map_name=three
)";

constexpr std::string_view kBadCanRest = R"([Map 000]
lookup_name=One
map_name=one
can_rest_here=Yes,No
)";

constexpr std::string_view kBadRandomStart = R"([Map 000]
lookup_name=One
map_name=one
random_start_point_0=elev=0, tile_num:123
)";

void require_parse_error_message(std::string_view input,
                                 const std::string &expected_message) {
  Parser parser;

  try {
    parser.parse(input);
    FAIL("expected ParseError");
  } catch (const ParseError &error) {
    REQUIRE(std::string{error.what()} == expected_message);
  }
}

} // namespace

TEST_CASE("parser reads multiple map sections", "[map_parser]") {
  Parser parser;
  const MapFile file = parser.parse(kSample);

  REQUIRE(file.maps.size() == 2);

  SECTION("first map parses common typed fields") {
    const auto &map = file.maps.at(0);

    REQUIRE(map.id == 0);
    REQUIRE(map.lookup_name ==
            std::optional<std::string>{"Desert Encounter 1"});
    REQUIRE(map.map_name == std::optional<std::string>{"desert1"});
    REQUIRE(map.music == std::optional<std::string>{"07desert"});
    REQUIRE(map.saved == std::optional<YesNo>{YesNo::No});
    REQUIRE(map.dead_bodies_age == std::optional<YesNo>{YesNo::No});

    REQUIRE(map.can_rest_here.has_value());
    REQUIRE((*map.can_rest_here)[0] == YesNo::No);
    REQUIRE((*map.can_rest_here)[1] == YesNo::No);
    REQUIRE((*map.can_rest_here)[2] == YesNo::No);
  }

  SECTION("ambient sfx parses as structured entries") {
    const auto &map = file.maps.at(0);

    REQUIRE(map.ambient_sfx.has_value());
    REQUIRE(map.ambient_sfx->size() == 6);

    REQUIRE(map.ambient_sfx->at(0).name == "gustwind");
    REQUIRE(map.ambient_sfx->at(0).weight == 20);

    REQUIRE(map.ambient_sfx->at(5).name == "vulture1");
    REQUIRE(map.ambient_sfx->at(5).weight == 15);
  }

  SECTION("random start points parse into indexed typed entries") {
    const auto &map = file.maps.at(0);

    REQUIRE(map.random_start_points.size() == 2);
    REQUIRE(map.random_start_points.contains(0));
    REQUIRE(map.random_start_points.contains(1));

    const auto &rsp0 = map.random_start_points.at(0);
    REQUIRE(rsp0.elevation == 0);
    REQUIRE(rsp0.tile_num == 19086);

    const auto &rsp1 = map.random_start_points.at(1);
    REQUIRE(rsp1.elevation == 0);
    REQUIRE(rsp1.tile_num == 17302);
  }

  SECTION("unknown fields are preserved raw") {
    const auto &map = file.maps.at(1);

    REQUIRE(map.pipboy_active == std::optional<YesNo>{YesNo::No});
    REQUIRE(map.saved == std::optional<YesNo>{YesNo::Yes});
    REQUIRE(map.raw_fields.contains("custom_field"));
    REQUIRE(map.raw_fields.at("custom_field") == "kept-as-raw");
  }
}

TEST_CASE("parser ignores full-line and inline comments", "[map_parser]") {
  Parser parser;

  const MapFile file = parser.parse(R"(
; leading comment

[Map 000]
lookup_name=Hello ; trailing comment
map_name=world
; whole line comment
saved=Yes
)");

  REQUIRE(file.maps.size() == 1);
  REQUIRE(file.maps[0].lookup_name == std::optional<std::string>{"Hello"});
  REQUIRE(file.maps[0].map_name == std::optional<std::string>{"world"});
  REQUIRE(file.maps[0].saved == std::optional<YesNo>{YesNo::Yes});
}

TEST_CASE("parser rejects malformed assignments", "[map_parser]") {
  require_parse_error_message(kBadMusicLine,
                              "line 4: expected key=value assignment");
}

TEST_CASE("parser rejects gaps in map numbering", "[map_parser]") {
  require_parse_error_message(
      kGapInIds, "line 5: map ids must be contiguous; expected 1 but got 2");
}

TEST_CASE("parser rejects malformed can_rest_here", "[map_parser]") {
  require_parse_error_message(
      kBadCanRest, "line 4: can_rest_here must contain exactly 3 values");
}

TEST_CASE("parser rejects malformed random_start_point", "[map_parser]") {
  Parser parser;

  REQUIRE_THROWS_AS(parser.parse(kBadRandomStart), ParseError);
}

TEST_CASE("parser rejects data before first map section", "[map_parser]") {
  require_parse_error_message(
      "lookup_name=oops\n",
      "line 1: key/value pair encountered before any [Map NNN] section");
}

TEST_CASE("state parses as YesNo using On/Off is not accepted",
          "[map_parser]") {
  Parser parser;

  REQUIRE_THROWS_AS(parser.parse(R"(
[Map 000]
lookup_name=Klamath Trapping Caves
map_name=klatrap
state=On
)"),
                    ParseError);
}
