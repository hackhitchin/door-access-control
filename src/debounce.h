#pragma once
#include <stdint.h>

class DebouncedBool
{
public:
    DebouncedBool(bool initialValue, uint32_t nowMs, uint32_t debounceMs)
        : stable_(initialValue),
          candidate_(initialValue),
          candidateSinceMs_(nowMs),
          debounceMs_(debounceMs)
    {
    }

    bool update(bool rawValue, uint32_t nowMs)
    {
        if (rawValue != candidate_) {
            candidate_ = rawValue;
            candidateSinceMs_ = nowMs;
        }

        if (candidate_ != stable_ &&
            static_cast<uint32_t>(nowMs - candidateSinceMs_) >= debounceMs_) {
            stable_ = candidate_;
            return true;
        }

        return false;
    }

    bool value() const { return stable_; }

private:
    bool stable_;
    bool candidate_;
    uint32_t candidateSinceMs_;
    uint32_t debounceMs_;
};

template <typename T>
class DebouncedValue
{
public:
    DebouncedValue(T initialValue, uint32_t nowMs, uint32_t debounceMs)
        : stable_(initialValue),
          candidate_(initialValue),
          candidateSinceMs_(nowMs),
          debounceMs_(debounceMs)
    {
    }

    bool update(T rawValue, uint32_t nowMs)
    {
        if (rawValue != candidate_) {
            candidate_ = rawValue;
            candidateSinceMs_ = nowMs;
        }

        if (candidate_ != stable_ &&
            static_cast<uint32_t>(nowMs - candidateSinceMs_) >= debounceMs_) {
            stable_ = candidate_;
            return true;
        }

        return false;
    }

    T value() const { return stable_; }

private:
    T stable_;
    T candidate_;
    uint32_t candidateSinceMs_;
    uint32_t debounceMs_;
};
