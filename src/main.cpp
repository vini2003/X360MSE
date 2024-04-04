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

void convert_file(const std::filesystem::path& file_path, const std::filesystem::path& output_path, const size_t index, const size_t total) {
    je2be::lce::Options convert_options;
    convert_options.fTempDirectory = mcfile::File::CreateTempDir(std::filesystem::temp_directory_path());

    defer {
        if (convert_options.fTempDirectory) {
            je2be::Fs::DeleteAll(*convert_options.fTempDirectory);
        }
    };

    std::wcout << tc::cyan << uc::RIGHTWARDS_HEAVY_ARROW << L" [" << index << L" / " << total << L"] Converting " << file_path.filename() << L"..." << tc::reset << std::endl;

    auto start_time = std::chrono::steady_clock::now();

    auto st = je2be::xbox360::Converter::Run(file_path, output_path, 8, convert_options, nullptr);

    auto end_time = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (auto err = st.error(); err) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wide_what = converter.from_bytes(err->fWhat);

        std::wcout << tc::red << L"[Error] Conversion failed: " << wide_what << tc::reset << std::endl;
        for (int i = err->fTrace.size() - 1; i >= 0; i--) {
            std::wstring wide_file = converter.from_bytes(err->fTrace[i].fFile);
            std::wstring wide_line = converter.from_bytes(err->fTrace[i].fLine);

            std::wcout << tc::reset << L"  " << wide_file << L":" << wide_line << tc::reset << std::endl;
        }
    } else {
        std::wcout << tc::green << uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW << L" [Success] Conversion completed successfully! (" << duration_ms << "ms)"  << tc::reset << std::endl;
    }
}

void extract_file_or_folder(const std::filesystem::path& file_path, const std::filesystem::path& output_directory, const bit7z::Bit7zLibrary& lib7z, const std::wregex& save_file_pattern, size_t index, size_t total) {
    using namespace bit7z;

    std::wcout << tc::cyan << uc::RIGHTWARDS_HEAVY_ARROW << L" [" << index << L" / " << total << L"] Processing " << file_path.filename() << L"..." << tc::reset << std::endl;

    auto was_archive = false;

    if (!std::filesystem::is_directory(file_path) && file_path.extension() != ".bin") {
        try {
            BitFileExtractor extractor{lib7z};
            BitArchiveReader archive_reader{lib7z, file_path.wstring()};

            was_archive = true;

            const auto items = archive_reader.items();

            std::vector<int> item_indices;

            for (auto i = 0; i < items.size(); ++i) {
                const auto& item = items.at(i);
                if (std::regex_match(item.name(), save_file_pattern)) {
                    item_indices.push_back(i);
                }
            }

            size_t counter = 0;

            for (const auto item_index : item_indices) {
                const auto& item = items.at(item_index);

                auto start_time = std::chrono::steady_clock::now();

                std::filesystem::path output_path = unique_path(output_directory, item.name());
                std::ofstream output_stream(output_path, std::ios::binary);
                extractor.extract(file_path.wstring(), output_stream, item.index());

                auto end_time = std::chrono::steady_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

                std::wcout << tc::white << uc::RIGHT_SHADED_WHITE_RIGHTWARDS_ARROW << L" [" << counter + 1 << L" / " << item_indices.size() + 1 << L"]" << L" Extracted " << item.name() << L" to " << output_path << L"! (" << duration_ms << "ms)" << tc::reset << std::endl;
                counter += 1;
            }
        } catch (const std::exception& e) {
            std::wcout << tc::red << L"[Error] An exception has occurred while processing " << file_path << L":" << tc::reset << std::endl;
            std::wcout << tc::red << e.what() << std::endl << std::endl;
        }
    }

    if (!was_archive) {
        if (std::regex_match(file_path.filename().wstring(), save_file_pattern)) {
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
            std::wcout << tc::red << "[Error] Input path is not a file or directory!" << tc::reset << std::endl;

            return EXIT_FAILURE;
        }

        std::wcout << std::endl;
        std::wcout << tc::bright_magenta << uc::RIGHTWARDS_HEAVY_ARROW << L" Processing " << files_to_process.size() << L" file(s)..." << tc::reset << std::endl;
        std::wcout << std::endl;

        size_t total = files_to_process.size();
        size_t index = 0;

        for (const auto& file_path : files_to_process) {
            index += 1;

            extract_file_or_folder(file_path, output_directory, lib7z, save_file_regex, index, total);
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
