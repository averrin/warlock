#pragma once

#include <game/systems/code_execution.hpp>
#include <map>
#include <sol/sol.hpp>
#include <string>

namespace warlock {

std::map<std::string, int> merge_spendable_maps(const std::map<std::string, int> &a,
                                                const std::map<std::string, int> &b);

std::map<std::string, int> parse_spendable_cost_table(sol::table spec);

std::map<std::string, int> component_script_spendable_cost(sol::state &lua,
                                                           const std::string &source);

std::map<std::string, int> blueprint_spendable_total(sol::state &lua,
                                                     CodeExecutionSystem &exec,
                                                     sol::table bp_spec);

} // namespace warlock
