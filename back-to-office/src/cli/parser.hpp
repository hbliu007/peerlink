#pragma once

#include <string>
#include <optional>

namespace bto::cli {
    struct Command {
        std::string name;
        std::string target;
        std::string remote_cmd;
        std::optional<int> forward_port;
        std::optional<std::string> file_path;
        bool serve = false;
        bool setup = false;
        bool version = false;
        bool help = false;
    };

    auto parse_arguments(int argc, char* argv[]) -> Command;
    void show_help();
    void show_version();
}
