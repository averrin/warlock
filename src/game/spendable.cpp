#include <game/spendable.hpp>
#include <sol/sol.hpp>

namespace warlock {

std::map<std::string, int> merge_spendable_maps(const std::map<std::string, int> &a,
                                                const std::map<std::string, int> &b) {
  std::map<std::string, int> out = a;
  for (const auto &[k, v] : b) {
    out[k] += v;
  }
  return out;
}

std::map<std::string, int> parse_spendable_cost_table(sol::table spec) {
  std::map<std::string, int> out;
  sol::optional<sol::table> t = spec["spendable_cost"];
  if (!t)
    return out;
  for (const auto &pair : t.value()) {
    if (!pair.first.is<std::string>())
      continue;
    std::string k = pair.first.as<std::string>();
    if (pair.second.is<int>()) {
      out[k] += pair.second.as<int>();
    } else if (pair.second.is<double>()) {
      out[k] += static_cast<int>(pair.second.as<double>());
    }
  }
  return out;
}

std::map<std::string, int> component_script_spendable_cost(sol::state &lua,
                                                           const std::string &source) {
  sol::table spec = lua.load(source).call();
  return parse_spendable_cost_table(spec);
}

std::map<std::string, int> blueprint_spendable_total(sol::state &lua,
                                                     CodeExecutionSystem &exec,
                                                     sol::table bp_spec) {
  auto out = parse_spendable_cost_table(bp_spec);
  sol::optional<sol::table> comps = bp_spec["components"];
  if (!comps)
    return out;
  for (const auto &pair : comps.value()) {
    if (!pair.second.is<std::string>())
      continue;
    std::string cn = pair.second.as<std::string>();
    if (exec.sources.count(cn) == 0)
      continue;
    out = merge_spendable_maps(out,
                               component_script_spendable_cost(lua, exec.getScript(cn)));
  }
  return out;
}

} // namespace warlock
