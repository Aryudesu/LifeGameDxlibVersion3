#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

class PerformanceLogger {
public:
    PerformanceLogger() {
        namespace fs = std::filesystem;
        std::error_code error;
        fs::create_directories("data/log/performance", error);
        if (error) return;

        const auto now = std::chrono::system_clock::now();
        const std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
        if (localtime_s(&localTime, &time) != 0) return;

        std::ostringstream fileName;
        fileName << "data/log/performance/performance_"
                 << std::put_time(&localTime, "%Y%m%d_%H%M%S") << ".csv";
        path_ = fileName.str();

        stream_.open(path_, std::ios::out | std::ios::trunc);
        if (!stream_) {
            path_.clear();
            return;
        }

        stream_ << "elapsed_seconds,generation,target_gen_per_s,actual_gen_per_s,fps,alive,chunks,"
                   "frame_ms,ms_per_generation,alive_per_chunk,frame_budget_usage_pct\n";
        stream_.flush();

        const auto start = Clock::now();
        runStart_ = start;
        sampleStart_ = start;
    }

    void record(double fps,
                int targetGenPerSecond,
                std::uint64_t generation,
                std::uint64_t alive,
                std::size_t chunks,
                bool paused) {
        if (!stream_) return;

        const auto now = Clock::now();

        if (paused) {
            resetSample(now, targetGenPerSecond, generation);
            return;
        }

        if (!sampleInitialized_ || targetGenPerSecond != sampleTargetGenPerSecond_) {
            resetSample(now, targetGenPerSecond, generation);
            sampleInitialized_ = true;
            return;
        }

        const double sampleSeconds = std::chrono::duration<double>(now - sampleStart_).count();
        if (sampleSeconds < SampleIntervalSeconds) return;

        const double elapsedSeconds = std::chrono::duration<double>(now - runStart_).count();
        const std::uint64_t advancedGenerations = generation - sampleGeneration_;
        const double actualGenPerSecond = advancedGenerations / sampleSeconds;
        const double frameMs = fps > 0.0 ? 1000.0 / fps : 0.0;
        const double msPerGeneration = advancedGenerations > 0
            ? sampleSeconds * 1000.0 / static_cast<double>(advancedGenerations)
            : 0.0;
        const double alivePerChunk = chunks > 0
            ? static_cast<double>(alive) / static_cast<double>(chunks)
            : 0.0;
        const double frameBudgetUsagePercent = frameMs / TargetFrameMs * 100.0;

        stream_ << std::fixed << std::setprecision(3)
                << elapsedSeconds << ','
                << generation << ','
                << targetGenPerSecond << ','
                << actualGenPerSecond << ','
                << fps << ','
                << alive << ','
                << chunks << ','
                << frameMs << ','
                << msPerGeneration << ','
                << alivePerChunk << ','
                << frameBudgetUsagePercent << '\n';
        stream_.flush();

        sampleStart_ = now;
        sampleGeneration_ = generation;
    }

    const std::string& path() const noexcept { return path_; }
    bool available() const noexcept { return stream_.is_open(); }

private:
    using Clock = std::chrono::steady_clock;
    static constexpr double SampleIntervalSeconds = 1.0;
    static constexpr double TargetFrameMs = 1000.0 / 60.0;

    void resetSample(Clock::time_point now, int targetGenPerSecond, std::uint64_t generation) noexcept {
        sampleStart_ = now;
        sampleGeneration_ = generation;
        sampleTargetGenPerSecond_ = targetGenPerSecond;
    }

    std::ofstream stream_;
    std::string path_;
    Clock::time_point runStart_{};
    Clock::time_point sampleStart_{};
    std::uint64_t sampleGeneration_ = 0;
    int sampleTargetGenPerSecond_ = 0;
    bool sampleInitialized_ = false;
};
