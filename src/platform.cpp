#include "platform.hpp"
#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
namespace xd {
std::vector<int> evdi_cards(const std::filesystem::path &root) {
    std::vector<int> result;
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator(root, error)) {
        const auto name = entry.path().filename().string();
        if (!name.starts_with("card") || name.size() == 4)
            continue;
        int number = 0;
        auto [end, ec] = std::from_chars(name.data() + 4, name.data() + name.size(), number);
        if (ec != std::errc{} || end != name.data() + name.size() || number < 0)
            continue;
        const auto driver = std::filesystem::canonical(entry.path() / "device/driver", error);
        if (!error && driver.filename() == "evdi")
            result.push_back(number);
    }
    std::sort(result.begin(), result.end());
    return result;
}
HostLock::HostLock(const std::filesystem::path &path) {
    fd_ = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    struct stat info{};
    if (fd_ < 0 || fstat(fd_, &info) != 0 || info.st_uid != getuid() || !S_ISREG(info.st_mode)) {
        if (fd_ >= 0)
            close(fd_);
        fd_ = -1;
        throw std::runtime_error("cannot create the WiredPad session lock");
    }
    if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        close(fd_);
        fd_ = -1;
        throw std::runtime_error("WiredPad is already running; stop the other host first");
    }
}
HostLock::~HostLock() {
    if (fd_ >= 0)
        close(fd_);
}
} // namespace xd
