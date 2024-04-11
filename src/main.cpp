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

// If on Windows, this must be included for '_setmode'.
#ifdef _WIN32
#include <fcntl.h>
#endif


#include <defer.hpp>

#include <pbar.hpp>

#include <fmt/core.h>
#include <fmt/xchar.h>
#include <fmt/color.h>

#include <je2be.hpp>

#include "../je2be-core/example/lce-progress.hpp"

#include "util.h"
#include "unicode.hpp"


// Shorten the namespaces for ease of use.
namespace uc = unicode;

/**
 * Generates a unique path in the specified directory with the specified name.
 *
 * If a file already exists with the same name, it will append a number to the name.
 * Otherwise, it will return the path as is.
 *
 * @param directory the directory to generate the unique path in.
 * @param name the name of the file.
 * @return the unique path.
 */
std::wstring unique_path(const std::filesystem::path& directory, const std::filesystem::path& name) {
    auto path = directory / name;

    if (!std::filesystem::exists(path)) {
        return path.wstring();
    }

    int counter = 1;

    auto stem = path.stem().string();
    auto extension = path.extension().string();

    while (std::filesystem::exists(path)) {
        counter += 1;

        // Append the counter to the stem of the path.
        // Ex.: "file (1).bin", "file (2).bin", etc.
        path = directory / std::filesystem::path(stem + " (" + std::to_string(counter) + ")" + extension);
    }

    return path.wstring();
}

/**
 * Converts the specified file from a Minecraft Xbox 360 Edition
 * to a Minecraft Java Edition save file.
 *
 * The conversion outputs the save as an uncompressed folder.
 *
 * @param file_path the file to convert.
 * @param output_path the directory to write to.
 * @param file_index the index of the file in the total.
 * @param file_total the total number of files to convert.
 */
void convert_file(const std::filesystem::path& file_path, const std::filesystem::path& output_path, const size_t file_index, const size_t file_total) {
    try {
        je2be::lce::Options convert_options;

        // Create a temporary directory to store the converted save.
        convert_options.fTempDirectory = mcfile::File::CreateTempDir(std::filesystem::temp_directory_path());

        // Delete the temporary directory after the conversion is complete.
        // defer uses RAII to ensure the temporary directory is deleted
        // when the function exits.
        defer {
            if (convert_options.fTempDirectory) {
                je2be::Fs::DeleteAll(*convert_options.fTempDirectory);
            }
        };

        fmt::print(L"\n");

        fmt::println(L"{}",
                     fmt::format(
                             L"{} {}",
                             fmt::styled(fmt::format(L"{} [{} / {}] ", uc::RIGHTWARDS_HEAVY_ARROW, file_index + 1, file_total), fmt::fg(fmt::color::cyan)),
                             fmt::styled(fmt::format(L"Converting {}...", file_path.filename().wstring()),fmt::fg(fmt::color::white))
                     ));

        fmt::print(L"\n");

        // Run the conversion and measure the time it takes.
        auto [duration_ms, status] = x360mse::util::run_measuring_ms<je2be::Status>([&]() {
            return je2be::xbox360::Converter::Run(file_path, output_path, 8, convert_options, nullptr);
        });

        if (auto err = status.error(); err) {
            fmt::println(L"{}",
                         fmt::format(
                                 L"{} {} {}",
                                 fmt::styled(fmt::format(L"{} [{} / {}] ", uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW, file_index + 1, file_total), fmt::fg(fmt::color::red)),
                                 fmt::styled(fmt::format(L"Failed to convert {}!", file_path.filename().wstring()),fmt::fg(fmt::color::white)),
                                 fmt::styled(fmt::format(L"({}ms)", duration_ms), fmt::fg(fmt::color::red))
                         ));
        } else {
            fmt::println(L"{}",
                         fmt::format(
                                 L"{} {} {}",
                                 fmt::styled(fmt::format(L"{} [{} / {}] ", uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW, file_index + 1, file_total), fmt::fg(fmt::color::green_yellow)),
                                 fmt::styled(fmt::format(L"Converted {}!", file_path.filename().wstring()),fmt::fg(fmt::color::white)),
                                    fmt::styled(fmt::format(L"({}ms)", duration_ms), fmt::fg(fmt::color::green_yellow))
                         ));
        }
    } catch (const std::exception& ex) {
        fmt::println(
                L"{}",
                fmt::styled(
                        std::format(L"{} {}:\n{}", uc::X, L"[Error] An exception has occurred!", x360mse::util::to_wstring(std::string(ex.what()))),
                        fmt::fg(fmt::color::red) | fmt::emphasis::bold
                ));
    }
}

static uint64_t pep_extraction_size = 0; // Total size of the extraction.
static size_t pep_prev_text_size = 0;
static std::chrono::time_point<std::chrono::steady_clock> pep_last_time = std::chrono::steady_clock::now();

void set_total_size(uint64_t size) {
    pep_extraction_size = size;
}

/**
 * Prints the extraction progress of a bit7z::BitFileExtractor
 * into stdout.
 *
 * Must call {#set_total_size} beforehand to set the total
 * extraction size.
 *
 * @param current_size the current extracted size.
 */
void print_extraction_progress(uint64_t current_size) {
    if (pep_extraction_size == 0) return; // Avoid division by zero.

    const auto current_time = std::chrono::steady_clock::now();

    // Only print if the delay is >50ms, as writing to stdout slows
    // down the conversion.
    if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - pep_last_time).count() < 50) {
        return;
    } else {
        pep_last_time = current_time;
    }

    // Calculate the percentage of the extraction.
    double percentage = static_cast<double>(current_size) / pep_extraction_size * 100.0;
    percentage = std::clamp(percentage, 0.0, 100.0);

    if (pep_prev_text_size != 0) {
        // Erase the previous text, based on the size of the previous text.
        fmt::print(L"{}", std::wstring(pep_prev_text_size, '\b'));
    }

    auto text = fmt::format(
            L"{} {} {}",
            fmt::styled(uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW, fmt::fg(fmt::color::white)),
            fmt::styled(fmt::format(L"[{:.2f}%]", percentage), fmt::fg(fmt::color::green_yellow)),
            fmt::styled(L"Extracting...", fmt::fg(fmt::color::white))
    );

    // Store the size of the text to erase it later.
    pep_prev_text_size = text.size();

    fmt::print(L"{}", text);
}

/**
 * Extracts the specified item from the archive.
 *
 * @param extractor the extractor to use.
 * @param archive_path the path to the archive. Note that this is the path to the archive itself,
 *                     <b>NOT</b> the path of the item inside the archive!
 * @param output_directory the directory to extract the item to.
 * @param info the information of the item to extract.
 */
void extract_from_archive(bit7z::BitFileExtractor &extractor, const std::filesystem::path &archive_path, const std::filesystem::path &output_directory, const bit7z::BitArchiveItemInfo &info) {
    const auto output_path = unique_path(output_directory, info.name());
    auto output_stream = std::ofstream { output_path, std::ios::binary };

    // Set the total callback function for the extraction.
    // This function is called with the total size of the file after extraction.
    extractor.setTotalCallback(set_total_size);

    // Set the progress callback function for the extraction.
    // This function is called with the current size of the file during extraction.
    extractor.setProgressCallback([&](uint64_t current_size) -> bool {
        print_extraction_progress(current_size);
        return true; // Continue the operation.
    });

    // Extract the item from the archive.
    extractor.extract(archive_path, output_stream, info.index());

    try {
#ifdef _WIN32
        HANDLE file_handle = CreateFileW(output_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file_handle == INVALID_HANDLE_VALUE) {
            return;
        }

        FILETIME creation_time;
        FILETIME last_access_time;
        FILETIME last_write_time;

        auto convert_to_filetime = [](const std::chrono::system_clock::time_point& tp) -> FILETIME {
            // Convert time_point to cast_time to use Windows epoch.
            auto cast_time = std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count() / 100;  // Convert to 100-nanoseconds units.

            // Adjust for the difference between UNIX epoch and Windows epoch.
            // Windows epoch starts 11644473600 seconds before UNIX epoch.
            const auto WINDOWS_TICKS = 116444736000000000LL;
            cast_time += WINDOWS_TICKS;

            FILETIME file_time;
            file_time.dwLowDateTime = static_cast<DWORD>(cast_time);
            file_time.dwHighDateTime = static_cast<DWORD>(cast_time >> 32);

            return file_time;
        };

        creation_time = convert_to_filetime(info.creationTime());
        last_access_time = convert_to_filetime(info.lastAccessTime());
        last_write_time = convert_to_filetime(info.lastWriteTime());

        if (!SetFileTime(file_handle, &creation_time, &last_access_time, &last_write_time)) {
            fmt::println(
                    L"{}",
                    fmt::styled(
                            std::format(L"{} {}:\n{}", uc::X, L"[Error] Failed to set file times!", x360mse::util::to_wstring(std::to_string(GetLastError()))),
                            fmt::fg(fmt::color::red) | fmt::emphasis::bold
                    ));
        }

        CloseHandle(file_handle);
#endif
    } catch (std::exception& ex) {
        fmt::println(
                L"{}",
                fmt::styled(
                        std::format(L"{} {}:\n{}", uc::X, L"[Error] An exception has occurred!", x360mse::util::to_wstring(std::string(ex.what()))),
                        fmt::fg(fmt::color::red) | fmt::emphasis::bold
                ));
    }
}

/**
 * Extracts all items from the specified archive.
 *
 * @param archive_path the path to the archive.
 * @param output_directory the directory to extract the items to.
 * @param lib7z the bit7z instance.
 * @param save_file_pattern the pattern to match save files.
 * @param file_index the index of the file in the total.
 * @param file_total the total number of files to extract.
 */
void extract_all_from_archive(const std::filesystem::path& archive_path, const std::filesystem::path& output_directory, const bit7z::Bit7zLibrary& lib7z, const std::wregex& save_file_pattern, size_t file_index, size_t file_total) {
    fmt::println(L"{}",
               fmt::format(
                       L"{} {}",
                       fmt::styled(fmt::format(L"{} [{} / {}] ", uc::RIGHTWARDS_HEAVY_ARROW, file_index + 1, file_total),fmt::fg(fmt::color::cyan)),
                       fmt::styled(fmt::format(L"Processing {}...", archive_path.filename().wstring()),fmt::fg(fmt::color::white))
               ));

    fmt::print(L"\n");

    // Hide the cursor to avoid flickering.
    fmt::print(L"\033[?25l");

    try {
        auto extractor = bit7z::BitFileExtractor {  lib7z };
        const auto reader = bit7z::BitArchiveReader {lib7z, archive_path.wstring() };

        std::vector<bit7z::BitArchiveItemInfo> filtered_infos;

        size_t filtered_info_total = 0;

        // Find and store the items in the archive matching save files.
        for (const auto& info : reader.items()) {
            if (std::regex_match(info.name(), save_file_pattern)) {
                filtered_infos.push_back(info);
                filtered_info_total += 1;
            }
        }

        size_t filtered_info_index = 0;

        for (const auto& info : filtered_infos) {
            // Extract the item from the archive and measure the time it takes.
            const auto duration_ms = x360mse::util::run_measuring_ms([&]() {
                extract_from_archive(extractor, archive_path, output_directory, info);
            });

            std::wcout << std::wstring(pep_prev_text_size, '\b');

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
        fmt::println(
                L"{}",
                fmt::styled(
                        std::format(L"{} {}:\n{}", uc::X, L"[Error] An exception has occurred!", x360mse::util::to_wstring(std::string(ex.what()))),
                        fmt::fg(fmt::color::red) | fmt::emphasis::bold
                ));
    }

    // Show the cursor again.
    fmt::print(L"\033[?25h");
}

/**
 * Copies all save files from the specified directory to the output directory.
 *
 * @param directory_path the directory to copy files from.
 * @param output_directory the directory to copy files to.
 * @param save_file_pattern the pattern to match save files.
 * @param directory_index the index of the directory in the total.
 * @param directory_total the total number of directories to copy from.
 */
void copy_all_from_directory(
        const std::filesystem::path& directory_path,
        const std::filesystem::path& output_directory,
        const std::wregex& minecraft_save_info_pattern,
        const std::wregex& save_file_pattern,
        size_t directory_index,
        size_t directory_total,
        std::map<std::wstring, je2be::xbox360::MinecraftSaveInfo::SaveBin>& save_bins
        ) {
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

        // Find and store all save files in the directory.
        for (const auto &entry: std::filesystem::directory_iterator(directory_path)) {
            if (std::filesystem::is_regular_file(entry)) {
                if (std::regex_match(entry.path().filename().wstring(), save_file_pattern)) {
                    filtered_paths.push_back(entry.path());
                    filtered_path_total += 1;
                }

                if (std::regex_match(entry.path().filename().wstring(), minecraft_save_info_pattern)) {
                    std::vector<je2be::xbox360::MinecraftSaveInfo::SaveBin> bins;

                    je2be::xbox360::MinecraftSaveInfo::Parse(entry.path(), bins);

                    for (const auto& bin : bins) {
                        // TODO: Handle the MinecraftSaveInfo files.
                        // Either copy them all with a unique path and then check the bin name, or do something else.
                        // I don't know anymore.
                        save_bins[x360mse::util::to_wstring(bin.fFileName)] = bin;
                    }
                }
            }
        }

        size_t filtered_path_index = 0;

        for (const auto& path : filtered_paths) {
            const auto output_path = unique_path(output_directory, path.filename());

            // Copy the file to the output directory and measure the time it takes.
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
        fmt::println(
                L"{}",
                fmt::styled(
                        std::format(L"{} {}:\n{}", uc::X, L"[Error] An exception has occurred!", x360mse::util::to_wstring(std::string(ex.what()))),
                        fmt::fg(fmt::color::red) | fmt::emphasis::bold
                ));
    }
}

/**
 * Copies the specified file to the output directory.
 *
 * @param file_path the file to copy.
 * @param output_directory the directory to copy the file to.
 */
void copy_file_(const std::filesystem::path& file_path, const std::filesystem::path& output_directory) {
    try {
        const auto output_path = unique_path(output_directory, file_path.filename());

        // Copy the file to the output directory and measure the time it takes.
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
        fmt::println(
                L"{}",
                fmt::styled(
                        std::format(L"{} {}:\n{}", uc::X, L"[Error] An exception has occurred!", x360mse::util::to_wstring(std::string(ex.what()))),
                        fmt::fg(fmt::color::red) | fmt::emphasis::bold
                ));
    }
}

int main(int argc, char* argv[]) {
    // If on Windows, this must be executed to allow the output stream of stdout to receive UTF-16 characters.
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
        fmt::println(L"{}", x360mse::util::to_wstring(options.help()));

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

    const auto minecraft_save_info_pattern = std::wregex { LR"(_MinecraftSaveInfo)" };
    const auto save_file_pattern = std::wregex {LR"(Save(.+)\.bin)"};
    const auto compression_file_pattern = std::wregex { LR"(.+(7z|ar|arj|bzip2|cab|chm|cpio|cramfs|deb|dmg|ext|fat|gpt|gzip|hfs|hxs|ihex|iso|lzh|lzma|mbr|msi|nsis|ntfs|qcow2|rar|rar5|rpm|squashfs|tar|udf|uefi|vdi|vhd|vmdk|wim|xar|xz|z|zip))" };

    try {
        bit7z::Bit7zLibrary lib7z{L"7z.dll"};

        if (!std::filesystem::exists(output_directory)) {
            // Create the output directory if it does not exist.
            std::filesystem::create_directories(output_directory);
        }

        std::map<std::wstring, je2be::xbox360::MinecraftSaveInfo> save_infos;

        if (std::filesystem::is_directory(input_path)) {
            // If the input path is a directory, copy all save files from the directory to the output directory.
            copy_all_from_directory(input_path, output_directory, save_file_pattern, 0, 1);
        } else if (std::filesystem::is_regular_file(input_path) && std::regex_match(input_path.filename().wstring(), save_file_pattern)) {
            // If the input path is a save file, copy the save file to the output directory.
            copy_file_(input_path, output_directory);
        } else if (std::filesystem::is_regular_file(input_path) && std::regex_match(input_path.filename().wstring(), compression_file_pattern)) {
            // If the input path is a compressed archive, extract all save files from the archive to the output directory.
            extract_all_from_archive(input_path, output_directory, lib7z, save_file_pattern, 0, 1);
        } else {
            fmt::println(
                    L"{}",
                    fmt::styled(
                            std::format(L"{} {}", uc::X, L"[Error] Input path is not a file or directory!"),
                            fmt::fg(fmt::color::red) | fmt::emphasis::bold
                    ));

            return EXIT_FAILURE;
        }

        std::vector<std::filesystem::path> save_file_paths;

        // Find and store all save files in the output directory.
        for (const auto& entry : std::filesystem::directory_iterator(output_directory)) {
            if (entry.path().extension() == ".bin") {
                save_file_paths.push_back(entry.path());
            }
        }

        size_t count = 0;

        for (const auto& save_path : save_file_paths) {
            auto save_output_path = output_directory / save_path.stem();

            if (!std::filesystem::exists(save_output_path)) {
                // Create the save's output directory if it does not exist.
                std::filesystem::create_directories(save_output_path);
            }

            // Convert the Minecraft Xbox 360 Edition save file to a Minecraft Java Edition save file.
            convert_file(save_path, save_output_path, count++, save_file_paths.size());
        }
    } catch (const std::exception& ex) {
        fmt::println(
                L"{}",
                fmt::styled(
                        std::format(L"{} {}:\n{}", uc::X, L"[Error] An exception has occurred!", x360mse::util::to_wstring(std::string(ex.what()))),
                        fmt::fg(fmt::color::red) | fmt::emphasis::bold
                ));

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
