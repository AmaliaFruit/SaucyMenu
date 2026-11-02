#pragma once

#include <string>

namespace saucy {

class ILog {
public:
    virtual ~ILog() = default;
    virtual void info(const std::string& message) = 0;
    virtual void warn(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;
};

class NullLog final : public ILog {
public:
    static NullLog& instance();
    void info(const std::string&) override {}
    void warn(const std::string&) override {}
    void error(const std::string&) override {}
};

}  // namespace saucy
