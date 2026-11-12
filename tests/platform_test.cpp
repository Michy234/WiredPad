#include "platform.hpp"
#include <iostream>
#include <unistd.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            throw std::runtime_error("failed: " #x);                                               \
    } while (0)
int main() {
    auto root =
        std::filesystem::temp_directory_path() / ("wiredpad-test-" + std::to_string(getpid()));
    try {
        std::filesystem::create_directories(root / "drivers/evdi");
        std::filesystem::create_directories(root / "drivers/xe");
        for (const auto &name : {"card0", "card2", "card1", "card3-DVI-I-1", "cardoops"}) {
            std::filesystem::create_directories(root / name / "device");
            std::filesystem::create_directory_symlink(
                root / (std::string(name) == "card1" ? "drivers/xe" : "drivers/evdi"),
                root / name / "device/driver");
        }
        CHECK(xd::evdi_cards(root) == std::vector<int>({0, 2}));
        CHECK(xd::evdi_cards(root / "missing").empty());
        {
            xd::HostLock first(root / "host.lock");
            bool rejected = false;
            try {
                xd::HostLock second(root / "host.lock");
            } catch (const std::runtime_error &) {
                rejected = true;
            }
            CHECK(rejected);
        }
        {
            xd::HostLock acquired_again(root / "host.lock");
        }
        std::filesystem::create_symlink(root / "host.lock", root / "unsafe.lock");
        bool rejected = false;
        try {
            xd::HostLock unsafe(root / "unsafe.lock");
        } catch (const std::runtime_error &) {
            rejected = true;
        }
        CHECK(rejected);
        std::filesystem::remove_all(root);
        std::cout << "EVDI detection and session locking passed\n";
    } catch (const std::exception &e) {
        std::filesystem::remove_all(root);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
