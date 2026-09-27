#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

// Writes a complete file in a unique temporary directory, then creates the
// requested name with create_hard_link. The link fails if any entry (including
// a dangling symlink) already owns that name, without replacing it.
class ExclusiveOutput {
public:
    explicit ExclusiveOutput(const std::filesystem::path& destination) : destination_(destination) {
        std::random_device random;
        for (int attempt = 0; attempt < 32; ++attempt) {
            temporary_directory_ = destination_;
            temporary_directory_ += ".tmp-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                "-" + std::to_string(random()) + "-" + std::to_string(attempt);
            std::error_code error;
            if (std::filesystem::create_directory(temporary_directory_, error)) {
                owns_temporary_ = true;
                try {
                    temporary_file_ = temporary_directory_ / "payload";
                    stream_.open(temporary_file_, std::ios::binary);
                    if (!stream_) throw std::runtime_error("cannot create temporary output file");
                } catch (...) {
                    cleanup();
                    throw;
                }
                return;
            }
            if (error && error != std::errc::file_exists)
                throw std::runtime_error("cannot create temporary output directory: " + error.message());
        }
        throw std::runtime_error("cannot reserve a temporary output directory");
    }

    ExclusiveOutput(const ExclusiveOutput&) = delete;
    ExclusiveOutput& operator=(const ExclusiveOutput&) = delete;
    ~ExclusiveOutput() { cleanup(); }

    std::ostream& stream() { return stream_; }

    void publish() {
        if (published_) throw std::runtime_error("output was already published");
        stream_.close();
        if (!stream_) throw std::runtime_error("cannot write output file");
        std::error_code error;
        std::filesystem::create_hard_link(temporary_file_, destination_, error);
        if (error) throw std::runtime_error("cannot publish output without replacing an existing path: " + error.message());
        published_ = true;
    }

private:
    void cleanup() noexcept {
        if (!owns_temporary_) return;
        if (stream_.is_open()) stream_.close();
        std::error_code ignored;
        std::filesystem::remove(temporary_file_, ignored);
        std::filesystem::remove(temporary_directory_, ignored);
    }

    std::filesystem::path destination_;
    std::filesystem::path temporary_directory_;
    std::filesystem::path temporary_file_;
    std::ofstream stream_;
    bool owns_temporary_ = false;
    bool published_ = false;
};
