#include <iostream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <vector>

#include <bit7z/bit7zlibrary.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitextractor.hpp>
#include <bit7z/bitfileextractor.hpp>

#include <cxxopts.hpp>

#include <termcolor/termcolor.hpp>

#ifdef _WIN32
#include <fcntl.h>
#endif

#include "unicode.hpp"

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

void process_file_or_folder(const std::filesystem::path& file_path, const std::filesystem::path& output_directory, const bit7z::Bit7zLibrary& lib7z, const std::wregex& savePattern, size_t index, size_t total) {
    using namespace bit7z;

    std::wcout << tc::cyan << uc::RIGHTWARDS_HEAVY_ARROW << L" [" << index << L" / " << total << L"] Processing " << file_path.filename() << L"..." << tc::reset << std::endl;

    auto was_archive = false;

    if (!std::filesystem::is_directory(file_path) && file_path.extension() != ".bin") {
        try {
            BitFileExtractor extractor{lib7z};
            BitArchiveReader archiveReader{lib7z, file_path.wstring()};

            was_archive = true;

            for (const auto &item: archiveReader.items()) {
                if (std::regex_match(item.name(), savePattern)) {
                    auto start_time = std::chrono::steady_clock::now();

                    std::filesystem::path output_path = unique_path(output_directory, item.name());
                    std::ofstream outStream(output_path, std::ios::binary);
                    extractor.extract(file_path.wstring(), outStream, item.index());

                    auto end_time = std::chrono::steady_clock::now();
                    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

                    std::wcout << tc::white << uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW << L" [" << index << L" / " << total << L"]" << L" Extracted " << item.name() << L" to " << output_path << L"! (" << duration_ms << "ms)" << tc::reset << std::endl;
                }
            }

        } catch (const std::exception& ex) {
            std::wcout << tc::red << L"[Error] An exception has occurred while processing " << file_path << L":" << tc::reset << std::endl;
            std::wcout << tc::red << ex.what() << std::endl << std::endl;
        }
    }

    if (!was_archive) {
        // For unzipped files matching the pattern directly in the input directory.
        if (std::regex_match(file_path.filename().wstring(), savePattern)) {
            auto start_time = std::chrono::steady_clock::now();

            std::filesystem::path output_path = unique_path(output_directory, file_path.filename());
            std::filesystem::copy(file_path, output_path);

            auto end_time = std::chrono::steady_clock::now();
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            std::wcout << tc::white << uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW << L" [" << index << L" / " << total << L"]" << L" Copied " << file_path.filename() << L" to " << output_path << L"! (" << duration_ms << "ms)" << tc::reset << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_U16TEXT);
#endif
    cxxopts::Options options("SaveExtractor", "Extract Xbox 360 Minecraft saves from a hard drive or compacted backup");

    options.add_options()
            ("i,input", "Input file or folder (can be HDD mounting point, eg. X:\\)", cxxopts::value<std::string>())
            ("o,output", "Output folder", cxxopts::value<std::string>())
            ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help") || !result.count("input") || !result.count("output")) {
        std::cout << options.help() << std::endl;
        return 0;
    }

    std::wcout << std::endl;
    std::wcout << tc::bright_magenta << uc::RIGHTWARDS_HEAVY_ARROW << " Welcome to X360MSE (Xbox 360 Minecraft Save Extractor)!" << tc::reset << std::endl;

    std::filesystem::path input_path = result["input"].as<std::string>();
    std::filesystem::path output_directory = result["output"].as<std::string>();

    std::wcout << std::endl;
    std::wcout << tc::cyan << uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW << " Extracting save file(s) from: " << input_path << tc::reset << std::endl;
    std::wcout << tc::cyan << uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW << " To: " << output_directory << tc::reset << std::endl;

    std::wregex save_file_regex{LR"(Save(.+)\.bin)"};

    try {
        bit7z::Bit7zLibrary lib7z{L"7z.dll"};

        if (!std::filesystem::exists(output_directory)) {
            std::filesystem::create_directories(output_directory);
        }

        std::vector<std::filesystem::path> files_to_process;

        if (std::filesystem::is_directory(input_path)) {
            for (const auto& entry : std::filesystem::directory_iterator(input_path)) {
                if (std::filesystem::is_regular_file(entry)) {
                    files_to_process.push_back(entry.path());
                }
            }
        } else if (std::filesystem::is_regular_file(input_path)) {
            files_to_process.push_back(input_path);
        } else {
            std::wcout << tc::red << "[Error] Input path is not a file or directory." << tc::reset << std::endl;
            return 1;
        }

        std::wcout << std::endl;
        std::wcout << tc::bright_magenta << uc::RIGHTWARDS_HEAVY_ARROW << L" Processing " << files_to_process.size() << L" file(s)..." << tc::reset << std::endl;
        std::wcout << std::endl;

        size_t total = files_to_process.size();
        size_t index = 0;

        for (const auto& file_path : files_to_process) {
            index += 1;
            process_file_or_folder(file_path, output_directory, lib7z, save_file_regex, index, total);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
