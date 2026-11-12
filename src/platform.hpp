#pragma once
#include <filesystem>
#include <vector>
namespace xd {
std::vector<int> evdi_cards(const std::filesystem::path &root = "/sys/class/drm");
class HostLock {
    int fd_ = -1;

  public:
    explicit HostLock(const std::filesystem::path &path);
    ~HostLock();
    HostLock(const HostLock &) = delete;
    HostLock &operator=(const HostLock &) = delete;
};
} // namespace xd
