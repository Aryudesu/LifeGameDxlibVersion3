#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

struct FramePerformanceMetrics {
    double simulationMs = 0.0;
    double boardRenderMs = 0.0;
    double uiRenderMs = 0.0;
    double frameWorkMs = 0.0;
    std::uint64_t visibleAlive = 0;
};

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
                   "simulation_ms,board_render_ms,ui_render_ms,frame_work_ms,visible_alive\n";
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
                bool paused,
                const FramePerformanceMetrics& metrics) {
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

        stream_ << std::fixed << std::setprecision(3)
                << elapsedSeconds << ','
                << generation << ','
                << targetGenPerSecond << ','
                << actualGenPerSecond << ','
                << fps << ','
                << alive << ','
                << chunks << ','
                << metrics.simulationMs << ','
                << metrics.boardRenderMs << ','
                << metrics.uiRenderMs << ','
                << metrics.frameWorkMs << ','
                << metrics.visibleAlive << '\n';
        stream_.flush();

        sampleStart_ = now;
        sampleGeneration_ = generation;
    }

    const std::string& path() const noexcept { return path_; }
    bool available() const noexcept { return stream_.is_open(); }

private:
    using Clock = std::chrono::steady_clock;
    static constexpr double SampleIntervalSeconds = 1.0;

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
