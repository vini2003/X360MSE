//
// Created by vini2003 on 04/04/2024.
//

#ifndef X360MSE_UTIL_H
#define X360MSE_UTIL_H

#include <chrono>

namespace x360mse::util {
    template<typename T>
    std::pair<long long, T> run_measuring_ms(std::function<T()> function) {
        const auto then = std::chrono::steady_clock::now();
        auto value = function();
        const auto now = std::chrono::steady_clock::now();
        return std::make_pair(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count(), value);
    }

    long long run_measuring_ms(const std::function<void()>& function) {
        const auto then = std::chrono::steady_clock::now();
        function();
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count();
    }
}


#endif //X360MSE_UTIL_H
