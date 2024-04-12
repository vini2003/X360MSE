#ifndef X360MSE_UTIL_H
#define X360MSE_UTIL_H

#include <chrono>
#include <string>

namespace x360mse::util {
    /**
     * Measure the time taken by a function to execute.
     *
     * @tparam T the return type of the function.
     * @param function the function to measure the time taken by.
     * @return pair of the time taken in milliseconds and the return value of the function.
     */
    template<typename T>
    std::pair<long long, T> run_measuring_ms(std::function<T()> function) {
        const auto then = std::chrono::steady_clock::now();
        auto value = function();
        const auto now = std::chrono::steady_clock::now();
        return std::make_pair(std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count(), value);
    }

    /**
     * Measure the time taken by a function to execute.
     *
     * @param function the function to measure the time taken by.
     * @return the time taken in milliseconds.
     */
    long long run_measuring_ms(const std::function<void()>& function) {
        const auto then = std::chrono::steady_clock::now();
        function();
        const auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count();
    }

    /**
     * Convert a string to a wide string.
     *
     * @param data the wide string to convert.
     * @return the converted string.
     */
    std::wstring to_wstring(const std::string& data) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(data);
    }

    /**
     * Convert a u16string to a wide string.
     */
    std::wstring to_wstring(const std::u16string& data) {
        return { data.begin(), data.end() };
    }

    /**
     * Convert a u16string to a string.
     */
    std::string to_string(const std::u16string& data) {
        return {data.begin(), data.end()};
    }
}

#endif
