#include "config.hpp"
#include "gui.hpp"
#include "nic_mgr.hpp"

#include <cstdio>
#include <iostream>

#include <sys/stat.h>

auto init_config_path()
{
    auto home_dotconfig_path = std::string{ getenv("HOME") } + "/.config";
    auto dir_path = home_dotconfig_path + "/ashmon";
    mkdir(dir_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    return dir_path + "/config";
}

int main()
{
    try
    {
        ashmon::config config{ init_config_path() };
        ashmon::nic_mgr nic_mgr{ &config };
        ashmon::gui gui{ &config, &nic_mgr };
        gui.run();
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exit with exception: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}