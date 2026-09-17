#pragma once
#include <string>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/gpio.h>

class GpioPTT {
public:
    GpioPTT() = default;
    ~GpioPTT() {
        close();
    }

    bool open(const std::string& chip, int line, bool active_low) {
        close();
        std::string path = chip.find('/') == std::string::npos ? "/dev/" + chip : chip;
        int cfd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (cfd < 0) {
            last_error_ = "Failed to open " + path + ": " + strerror(errno);
            return false;
        }
        struct gpio_v2_line_request req;
        std::memset(&req, 0, sizeof(req));
        req.offsets[0] = line;
        req.num_lines = 1;
        std::strncpy(req.consumer, "modem73 ptt", sizeof(req.consumer) - 1);
        req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT | (active_low ? GPIO_V2_LINE_FLAG_ACTIVE_LOW : 0);
        req.config.num_attrs = 1;
        req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
        req.config.attrs[0].attr.values = 0;
        req.config.attrs[0].mask = 1;
        int rc = ioctl(cfd, GPIO_V2_GET_LINE_IOCTL, &req);
        int err = errno;
        ::close(cfd);
        if (rc < 0 || req.fd < 0) {
            last_error_ = "GPIO line " + std::to_string(line) + " on " + path + ": " + strerror(err);
            return false;
        }
        fd_ = req.fd;
        return set(false);
    }

    bool set_ptt(bool on) {
        return set(on);
    }

    bool is_open() const { return fd_ >= 0; }
    const std::string& last_error() const { return last_error_; }

    void close() {
        if (fd_ >= 0) {
            set(false);
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    bool set(bool on) {
        if (fd_ < 0) {
            last_error_ = "GPIO line not open";
            return false;
        }
        struct gpio_v2_line_values v;
        std::memset(&v, 0, sizeof(v));
        v.mask = 1;
        v.bits = on ? 1 : 0;
        if (ioctl(fd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &v) < 0) {
            last_error_ = std::string("GPIO set failed: ") + strerror(errno);
            return false;
        }
        return true;
    }

    int fd_ = -1;
    std::string last_error_;
};
