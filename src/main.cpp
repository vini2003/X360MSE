#include <iostream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>

#include "bit7z/bit7zlibrary.hpp"
#include "bit7z/bitarchivereader.hpp"
#include "bit7z/bitextractor.hpp"
#include "bit7z/bitfileextractor.hpp"

#include <cxxopts.hpp>

#include <termcolor/termcolor.hpp>

// If on Windows, this must be included for '_setmode'.
#ifdef _WIN32
#include <fcntl.h>
#endif

#include <je2be.hpp>

#include <defer.hpp>

#include <pbar.hpp>

#include <fmt/core.h>
#include <fmt/xchar.h>
#include <fmt/color.h>

#include "util.h"
#include "unicode.hpp"
#include "../je2be-core/example/lce-progress.hpp"

// Shorten the namespaces for ease of use.
namespace tc = termcolor;
namespace uc = unicode;

std::string unique_path(const std::filesystem::path& directory, const std::filesystem::path& name) {
    auto path = directory / name;

    if (!std::filesystem::exists(path)) {
        return path.string();
    }

    int counter = 1;

    auto stem = path.stem().string();
    auto extension = path.extension().string();

    while (std::filesystem::exists(path)) {
        counter += 1;
        path = directory / std::filesystem::path(stem + " (" + std::to_string(counter) + ")" + extension);
    }

    return path.string();
}

void convert_file(const std::filesystem::path& file_path, const std::filesystem::path& output_path, const size_t file_index, const size_t file_total) {
    try {
        je2be::lce::Options convert_options;
        convert_options.fTempDirectory = mcfile::File::CreateTempDir(std::filesystem::temp_directory_path());

        defer {
            if (convert_options.fTempDirectory) {
                je2be::Fs::DeleteAll(*convert_options.fTempDirectory);
            }
        };

        fmt::println(L"{}",
                     fmt::format(
                             L"{} {}",
                             fmt::styled(fmt::format(L"{} [{} / {}] ", uc::RIGHTWARDS_HEAVY_ARROW, file_index, file_total), fmt::fg(fmt::color::cyan)),
                             fmt::styled(fmt::format(L"Converting {}...", file_path.filename().wstring()),fmt::fg(fmt::color::white))
                     ));

        auto [duration_ms, status] = x360mse::util::run_measuring_ms<je2be::Status>([&]() {
            return je2be::xbox360::Converter::Run(file_path, output_path, 8, convert_options, nullptr);
        });

        if (auto err = status.error(); err) {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            std::wstring wide_what = converter.from_bytes(err->fWhat);

            std::wcout << tc::red << L"[Error] Conversion failed: " << wide_what << tc::reset << std::endl;
            for (int i = err->fTrace.size() - 1; i >= 0; i--) {
                std::wstring wide_file = converter.from_bytes(err->fTrace[i].fFile);
                std::wstring wide_line = converter.from_bytes(err->fTrace[i].fLine);

                std::wcout << tc::reset << L"  " << wide_file << L":" << wide_line << tc::reset << std::endl;
            }
        } else {
            std::wcout << tc::green << uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW
                       << L" [Success] Conversion completed successfully! (" << duration_ms << "ms)" << tc::reset
                       << std::endl;
        }
    } catch (const std::exception& e) {
        std::wcout << tc::red << L"[Error] An exception has occurred while converting item: " << e.what() << tc::reset << std::endl;
    }
}

static uint64_t total_size = 0; // Total size of the extraction
static size_t prev_text_size = 0;
static std::chrono::time_point<std::chrono::steady_clock> last_time = std::chrono::steady_clock::now();

void set_total_size(uint64_t size) {
    total_size = size;
}

void print_extraction_progress(uint64_t current_size) {
    if (total_size == 0) return; // Avoid division by zero

    const auto current_time = std::chrono::steady_clock::now();

    // if less than 50ms passed, skip
    if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time).count() < 50) {
        return;
    } else {
        last_time = current_time;
    }

    double percentage = static_cast<double>(current_size) / total_size * 100.0;
    percentage = std::clamp(percentage, 0.0, 100.0);

    if (prev_text_size != 0) {
        std::wcout << std::wstring(prev_text_size, '\b'); // Adjust width based on your output length
    }

    auto text = fmt::format(
            L"{} {} {}",
            fmt::styled(uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW, fmt::fg(fmt::color::white)),
            fmt::styled(fmt::format(L"[{:.2f}%]", percentage), fmt::fg(fmt::color::green_yellow)),
            fmt::styled(L"Extracting...", fmt::fg(fmt::color::white))
    );

    prev_text_size = text.size();

    fmt::print(L"{}", text);
}

void extract_from_archive(bit7z::BitFileExtractor &extractor, const std::filesystem::path &archive_path, const std::filesystem::path &output_directory, const bit7z::BitArchiveItemInfo &info) {
    const auto output_path = unique_path(output_directory, info.name());
    auto output_stream = std::ofstream { output_path, std::ios::binary };

    // Assuming there's a way to set a TotalCallback, not shown in provided callbacks
    extractor.setTotalCallback(set_total_size); // This might need to be adapted based on actual API

    extractor.setProgressCallback([&](uint64_t current_size) -> bool {
        print_extraction_progress(current_size);
        return true; // Return true to continue operation, false to cancel
    });

    extractor.extract(archive_path, output_stream, info.index());
}

void extract_all_from_archive(const std::filesystem::path& archive_path, const std::filesystem::path& output_directory, const bit7z::Bit7zLibrary& lib7z, const std::wregex& save_file_pattern, size_t file_index, size_t file_total) {
    fmt::println(L"{}",
               fmt::format(
                       L"{} {}",
                       fmt::styled(fmt::format(L"{} [{} / {}] ", uc::RIGHTWARDS_HEAVY_ARROW, file_index + 1, file_total),fmt::fg(fmt::color::cyan)),
                       fmt::styled(fmt::format(L"Processing {}...", archive_path.filename().wstring()),fmt::fg(fmt::color::white))
               ));

    fmt::print(L"\n");

    try {
        auto extractor = bit7z::BitFileExtractor {  lib7z };
        const auto reader = bit7z::BitArchiveReader {lib7z, archive_path.wstring() };

        std::vector<bit7z::BitArchiveItemInfo> filtered_infos;

        size_t filtered_info_total = 0;

        for (const auto& info : reader.items()) {
            if (std::regex_match(info.name(), save_file_pattern)) {
                filtered_infos.push_back(info);
                filtered_info_total += 1;
            }
        }

        size_t filtered_info_index = 0;

        for (const auto& info : filtered_infos) {
            const auto duration_ms = x360mse::util::run_measuring_ms([&]() {
                extract_from_archive(extractor, archive_path, output_directory, info);
            });

            std::wcout << std::wstring(prev_text_size, '\b'); // Adjust width based on your output length

            fmt::println(L"{}",
                       fmt::format(
                               L"{} {} {}",
                               fmt::styled(fmt::format(L"{} [{} / {}]", uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW, filtered_info_index + 1, filtered_info_total), fmt::fg(fmt::color::green_yellow)),
                               fmt::styled(fmt::format(L"Extracted {} to {}!", info.name(), output_directory.wstring()), fmt::fg(fmt::color::white)),
                               fmt::styled(fmt::format(L"({}ms)", duration_ms), fmt::fg(fmt::color::green_yellow))
                       ));

            filtered_info_index += 1;
        }
    } catch (std::exception& ex) {

    }
}

void copy_all_from_directory(const std::filesystem::path& directory_path, const std::filesystem::path& output_directory, const std::wregex& save_file_pattern, size_t directory_index, size_t directory_total) {
    fmt::println(L"{}",
               fmt::format(
                       L"{} {}",
                       fmt::styled(fmt::format(L"{} [{} / {}] ", uc::RIGHTWARDS_HEAVY_ARROW, directory_index + 1, directory_total), fmt::fg(fmt::color::cyan)),
                       fmt::styled(fmt::format(L"Processing {}...", directory_path.filename().wstring()),fmt::fg(fmt::color::white))
               ));

    fmt::print(L"\n");

    try {
        std::vector<std::filesystem::path> filtered_paths;

        size_t filtered_path_total = 0;

        for (const auto &entry: std::filesystem::directory_iterator(directory_path)) {
            if (std::filesystem::is_regular_file(entry)) {
                if (std::regex_match(entry.path().filename().wstring(), save_file_pattern)) {
                    filtered_paths.push_back(entry.path());
                    filtered_path_total += 1;
                }
            }
        }

        size_t filtered_path_index = 0;

        for (const auto& path : filtered_paths) {
            const auto output_path = unique_path(output_directory, path.filename());

            const auto duration_ms = x360mse::util::run_measuring_ms([&]() {
                std::filesystem::copy(path, output_path);
            });

            fmt::println(L"{}",
                       fmt::format(
                               L"{} {} {}",
                               fmt::styled(fmt::format(L"{} [{} / {}]", uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW, filtered_path_index + 1, filtered_path_total), fmt::fg(fmt::color::green_yellow)),
                               fmt::styled(fmt::format(L"Copied {} to {}!", path.filename().wstring(), output_directory.wstring()), fmt::fg(fmt::color::white)),
                               fmt::styled(fmt::format(L"({}ms)", duration_ms), fmt::fg(fmt::color::green_yellow))
                       ));

            filtered_path_index += 1;
        }
    } catch (std::exception& ex) {

    }
}

void copy_file_(const std::filesystem::path& file_path, const std::filesystem::path& output_directory) {
    try {
        const auto output_path = unique_path(output_directory, file_path.filename());

        const auto duration_ms = x360mse::util::run_measuring_ms([&]() {
            std::filesystem::copy(file_path, output_path);
        });

        fmt::println(L"{}",
                   fmt::format(
                           L"{} {} {}",
                           fmt::styled(fmt::format(L"{} [{} / {}]", uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW, 1, 1), fmt::fg(fmt::color::green_yellow)),
                           fmt::styled(fmt::format(L"Copied {} to {}!", file_path.filename().wstring(), output_directory.wstring()), fmt::fg(fmt::color::white)),
                           fmt::styled(fmt::format(L"({}ms)", duration_ms), fmt::fg(fmt::color::green_yellow))
                   ));
    } catch (std::exception& ex) {

    }
}

int main(int argc, char* argv[]) {
    // If on Windows, this must be executed to allow the output stream of std::wcout to receive UTF-16 characters.
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_U16TEXT);
#endif
    cxxopts::Options options("X360MSE", "Extract Xbox 360 Minecraft saves from a hard drive or compacted backup");

    options.add_options()
            ("i,input", "Input file or folder (can be HDD mounting point, eg. X:\\)", cxxopts::value<std::string>())
            ("o,output", "Output folder", cxxopts::value<std::string>())
            ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help") || !result.count("input") || !result.count("output")) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wide_help = converter.from_bytes(options.help());

        std::wcout << wide_help << std::endl;

        return EXIT_SUCCESS;
    }

    fmt::print(L"\n");
    fmt::println(L"{}", fmt::styled(fmt::format(L"{} {}", uc::RIGHTWARDS_HEAVY_ARROW, L"Welcome to Xbox 360 Minecraft Save Extractor! (X360MSE)"), fmt::fg(fmt::color::white)));
    fmt::print(L"\n");

    std::filesystem::path input_path = result["input"].as<std::string>();
    std::filesystem::path output_directory = result["output"].as<std::string>();

    fmt::println(L"{}",
               fmt::format(
                       L"{} {}: {}",
                       fmt::styled(uc::RIGHTWARDS_HEAVY_ARROW, fmt::fg(fmt::color::green_yellow)),
                       fmt::styled(L"Extracting save file(s) from", fmt::fg(fmt::color::white)),
                       fmt::styled( input_path.wstring(), fmt::fg(fmt::color::green_yellow))
               )
    );

    fmt::println(L"{}",
               fmt::format(
                       L"{} {}: {}",
                       fmt::styled(uc::RIGHTWARDS_HEAVY_ARROW, fmt::fg(fmt::color::green_yellow)),
                       fmt::styled(L"Into", fmt::fg(fmt::color::white)),
                       fmt::styled( output_directory.wstring(), fmt::fg(fmt::color::green_yellow))
               )
    );

    fmt::print(L"\n");

    const auto save_file_pattern = std::wregex {LR"(Save(.+)\.bin)"};
    const auto compression_file_pattern = std::wregex { LR"(.+(7z|ar|arj|bzip2|cab|chm|cpio|cramfs|deb|dmg|ext|fat|gpt|gzip|hfs|hxs|ihex|iso|lzh|lzma|mbr|msi|nsis|ntfs|qcow2|rar|rar5|rpm|squashfs|tar|udf|uefi|vdi|vhd|vmdk|wim|xar|xz|z|zip))" };

    try {
        bit7z::Bit7zLibrary lib7z{L"7z.dll"};

        if (!std::filesystem::exists(output_directory)) {
            std::filesystem::create_directories(output_directory);
        }

        if (std::filesystem::is_directory(input_path)) {
            copy_all_from_directory(input_path, output_directory, save_file_pattern, 0, 1);
        } else if (std::filesystem::is_regular_file(input_path) && std::regex_match(input_path.filename().wstring(), save_file_pattern)) {
            copy_file_(input_path, output_directory);
        } else if (std::filesystem::is_regular_file(input_path) && std::regex_match(input_path.filename().wstring(), compression_file_pattern)) {
            extract_all_from_archive(input_path, output_directory, lib7z, save_file_pattern, 0, 1);
        } else {
            std::wcout << tc::red << "[Error] Input path is not a file or directory!" << tc::reset << std::endl;

            return EXIT_FAILURE;
        }

        std::vector<std::filesystem::path> save_file_paths;

        for (const auto& entry : std::filesystem::directory_iterator(output_directory)) {
            if (entry.path().extension() == ".bin") {
                save_file_paths.push_back(entry.path());
            }
        }

        size_t count = 0;

        for (const auto& save_path : save_file_paths) {
            auto save_output_path = output_directory / save_path.stem();

            if (!std::filesystem::exists(save_output_path)) {
                std::filesystem::create_directories(save_output_path);
            }

            convert_file(save_path, save_output_path, count++ + 1, save_file_paths.size());
        }
    } catch (const std::exception& e) {
        std::wcout << tc::red << L"[Error] An exception has occurred:" << tc::reset << std::endl;
        std::wcout << tc::red << e.what() << tc::reset << std::endl;

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
