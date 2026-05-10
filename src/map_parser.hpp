#pragma once

#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fallout2::maps {

struct ParseError : public std::runtime_error {
  std::size_t line;

  ParseError(std::size_t line_number, const std::string &message)
      : std::runtime_error("line " + std::to_string(line_number) + ": " +
                           message),
        line(line_number) {}
};

enum class YesNo {
  No,
  Yes,
};

struct AmbientSfxEntry {
  std::string name;
  int weight{};
};

struct RandomStartPoint {
  int elevation{};
  int tile_num{};
};

struct MapEntry {
  int id{};

  std::optional<std::string> lookup_name;
  std::optional<std::string> map_name;
  std::optional<std::string> music;
  std::optional<std::vector<AmbientSfxEntry>> ambient_sfx;
  std::optional<YesNo> saved;
  std::optional<YesNo> dead_bodies_age;
  std::optional<YesNo> pipboy_active;
  std::optional<YesNo> state;
  std::optional<std::array<YesNo, 3>> can_rest_here;

  std::unordered_map<int, RandomStartPoint> random_start_points;
  std::unordered_map<std::string, std::string> raw_fields;
};

struct MapFile {
  std::vector<MapEntry> maps;
};

namespace detail {

inline std::string trim(std::string_view sv) {
  std::size_t begin = 0;
  while (begin < sv.size() &&
         std::isspace(static_cast<unsigned char>(sv[begin])) != 0) {
    ++begin;
  }

  std::size_t end = sv.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(sv[end - 1])) != 0) {
    --end;
  }

  return std::string{sv.substr(begin, end - begin)};
}

inline std::string strip_inline_comment(std::string_view sv) {
  const auto pos = sv.find(';');
  if (pos == std::string_view::npos) {
    return trim(sv);
  }
  return trim(sv.substr(0, pos));
}

inline bool is_blank_or_comment(std::string_view sv) {
  const auto t = trim(sv);
  return t.empty() || t[0] == ';';
}

inline YesNo parse_yes_no(std::string_view sv, std::size_t line) {
  const std::string value = trim(sv);
  if (value == "Yes") {
    return YesNo::Yes;
  }
  if (value == "No") {
    return YesNo::No;
  }
  throw ParseError(line, "expected Yes or No, got '" + value + "'");
}

inline std::array<YesNo, 3> parse_can_rest_here(std::string_view sv,
                                                std::size_t line) {
  std::array<YesNo, 3> result{};
  std::stringstream ss(trim(sv));
  std::string part;
  std::size_t i = 0;

  while (std::getline(ss, part, ',')) {
    if (i >= result.size()) {
      throw ParseError(line, "can_rest_here must contain exactly 3 values");
    }
    result[i++] = parse_yes_no(part, line);
  }

  if (i != result.size()) {
    throw ParseError(line, "can_rest_here must contain exactly 3 values");
  }

  return result;
}

inline std::vector<AmbientSfxEntry> parse_ambient_sfx(std::string_view sv,
                                                      std::size_t line) {
  std::vector<AmbientSfxEntry> result;
  std::stringstream ss(trim(sv));
  std::string item;

  while (std::getline(ss, item, ',')) {
    const std::string t = trim(item);
    if (t.empty()) {
      continue;
    }

    const auto colon = t.find(':');
    if (colon == std::string::npos) {
      throw ParseError(line, "ambient_sfx entry missing ':' in '" + t + "'");
    }

    const std::string name = trim(std::string_view{t}.substr(0, colon));
    const std::string weight_text = trim(std::string_view{t}.substr(colon + 1));

    if (name.empty()) {
      throw ParseError(line, "ambient_sfx entry has empty name");
    }

    int weight = 0;
    try {
      std::size_t idx = 0;
      weight = std::stoi(weight_text, &idx, 10);
      if (idx != weight_text.size()) {
        throw ParseError(
            line, "ambient_sfx weight is not a valid integer in '" + t + "'");
      }
    } catch (const std::invalid_argument &) {
      throw ParseError(line, "ambient_sfx weight is not a valid integer in '" +
                                 t + "'");
    } catch (const std::out_of_range &) {
      throw ParseError(line,
                       "ambient_sfx weight is out of range in '" + t + "'");
    }

    result.push_back(AmbientSfxEntry{name, weight});
  }

  return result;
}

inline RandomStartPoint parse_random_start_point(std::string_view sv,
                                                 std::size_t line) {
  std::stringstream ss(trim(sv));
  std::string first;
  std::string second;

  if (!std::getline(ss, first, ',')) {
    throw ParseError(line, "random_start_point missing elev component");
  }
  if (!std::getline(ss, second, ',')) {
    throw ParseError(line, "random_start_point missing tile_num component");
  }

  const std::string lhs = trim(first);
  const std::string rhs = trim(second);

  const auto colon1 = lhs.find(':');
  const auto colon2 = rhs.find(':');

  if (colon1 == std::string::npos || colon2 == std::string::npos) {
    throw ParseError(line,
                     "random_start_point must look like 'elev:X, tile_num:Y'");
  }

  const std::string key1 = trim(std::string_view{lhs}.substr(0, colon1));
  const std::string val1 = trim(std::string_view{lhs}.substr(colon1 + 1));
  const std::string key2 = trim(std::string_view{rhs}.substr(0, colon2));
  const std::string val2 = trim(std::string_view{rhs}.substr(colon2 + 1));

  if (key1 != "elev" || key2 != "tile_num") {
    throw ParseError(line,
                     "random_start_point must use keys 'elev' and 'tile_num'");
  }

  int elev = 0;
  int tile_num = 0;
  try {
    std::size_t idx1 = 0;
    elev = std::stoi(val1, &idx1, 10);
    if (idx1 != val1.size()) {
      throw ParseError(line, "invalid elevation '" + val1 + "'");
    }

    std::size_t idx2 = 0;
    tile_num = std::stoi(val2, &idx2, 10);
    if (idx2 != val2.size()) {
      throw ParseError(line, "invalid tile_num '" + val2 + "'");
    }
  } catch (const std::invalid_argument &) {
    throw ParseError(line, "random_start_point contains a non-integer value");
  } catch (const std::out_of_range &) {
    throw ParseError(line, "random_start_point integer value out of range");
  }

  return RandomStartPoint{elev, tile_num};
}

inline int parse_map_id(std::string_view line, std::size_t line_number) {
  static const std::regex re(R"(^\[\s*Map\s+(\d{3})\s*\]$)");
  std::smatch match;
  const std::string text = trim(line);

  if (!std::regex_match(text, match, re)) {
    throw ParseError(line_number, "invalid section header '" + text + "'");
  }

  return std::stoi(match[1].str());
}

inline int parse_random_start_index(const std::string &key) {
  static const std::regex re(R"(^random_start_point_(\d+)$)");
  std::smatch match;
  if (!std::regex_match(key, match, re)) {
    return -1;
  }
  return std::stoi(match[1].str());
}

} // namespace detail

class Parser {
public:
  [[nodiscard]] MapFile parse(std::string_view text) const {
    MapFile file;
    std::istringstream in(std::string{text});
    std::string raw_line;
    std::size_t line_number = 0;
    MapEntry *current = nullptr;
    int expected_map_id = 0;

    while (std::getline(in, raw_line)) {
      ++line_number;

      if (!raw_line.empty() && raw_line.back() == '\r') {
        raw_line.pop_back();
      }

      if (detail::is_blank_or_comment(raw_line)) {
        continue;
      }

      const std::string no_comment = detail::strip_inline_comment(raw_line);
      if (no_comment.empty()) {
        continue;
      }

      if (!no_comment.empty() && no_comment.front() == '[') {
        const int map_id = detail::parse_map_id(no_comment, line_number);
        if (map_id != expected_map_id) {
          throw ParseError(line_number,
                           "map ids must be contiguous; expected " +
                               std::to_string(expected_map_id) + " but got " +
                               std::to_string(map_id));
        }

        file.maps.push_back(MapEntry{});
        current = &file.maps.back();
        current->id = map_id;
        ++expected_map_id;
        continue;
      }

      if (current == nullptr) {
        throw ParseError(
            line_number,
            "key/value pair encountered before any [Map NNN] section");
      }

      const auto eq_pos = no_comment.find('=');
      if (eq_pos == std::string::npos) {
        throw ParseError(line_number, "expected key=value assignment");
      }

      const std::string key =
          detail::trim(std::string_view{no_comment}.substr(0, eq_pos));
      const std::string value =
          detail::trim(std::string_view{no_comment}.substr(eq_pos + 1));

      if (key.empty()) {
        throw ParseError(line_number, "empty key in assignment");
      }

      if (key == "lookup_name") {
        current->lookup_name = value;
      } else if (key == "map_name") {
        current->map_name = value;
      } else if (key == "music") {
        current->music = value;
      } else if (key == "ambient_sfx") {
        current->ambient_sfx = detail::parse_ambient_sfx(value, line_number);
      } else if (key == "saved") {
        current->saved = detail::parse_yes_no(value, line_number);
      } else if (key == "dead_bodies_age") {
        current->dead_bodies_age = detail::parse_yes_no(value, line_number);
      } else if (key == "pipboy_active") {
        current->pipboy_active = detail::parse_yes_no(value, line_number);
      } else if (key == "state") {
        current->state = detail::parse_yes_no(value, line_number);
      } else if (key == "can_rest_here") {
        current->can_rest_here =
            detail::parse_can_rest_here(value, line_number);
      } else {
        const int random_index = detail::parse_random_start_index(key);
        if (random_index >= 0) {
          current->random_start_points[random_index] =
              detail::parse_random_start_point(value, line_number);
        } else {
          current->raw_fields[key] = value;
        }
      }
    }

    return file;
  }
};

inline bool operator==(YesNo lhs, YesNo rhs) {
  return static_cast<int>(lhs) == static_cast<int>(rhs);
}

} // namespace fallout2::maps
