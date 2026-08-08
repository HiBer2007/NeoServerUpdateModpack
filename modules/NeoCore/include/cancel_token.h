#pragma once

#include <atomic>

namespace NeoCore {

class CancelToken {
public:
    CancelToken() : cancelled_(false) {}

    void request_cancel() { cancelled_.store(true); }

    bool is_cancelled() const { return cancelled_.load(); }

    void reset() { cancelled_.store(false); }

private:
    std::atomic<bool> cancelled_;
};

} // namespace NeoCore
