#pragma once

#include <string>

namespace nebula::browser {

std::string NormalizeNavigationInput(const std::string& input);
std::string JsonEscape(const std::string& value);

}  // namespace nebula::browser
