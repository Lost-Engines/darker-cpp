#pragma once

#include <filesystem>

namespace darker::resources { class archive_set; }

void check_hangar_flight(darker::resources::archive_set const &archives, std::filesystem::path const &trace = {});
