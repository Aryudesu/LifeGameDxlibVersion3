#pragma once

#include "InfiniteLifeBoard.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

class EditHistory {
public:
    using Coord = InfiniteLifeBoard::Coord;

    explicit EditHistory(std::size_t maxCommands = 256)
        : maxCommands_(maxCommands) {}

    void begin() {
        if (active_) return;
        active_ = true;
        current_.clear();
        currentIndex_.clear();
    }

    bool active() const noexcept { return active_; }

    void setAlive(InfiniteLifeBoard& board, Coord x, Coord y, bool alive) {
        if (!active_) begin();

        const bool beforeNow = board.isAlive(x, y);
        if (beforeNow == alive) return;

        const CellKey key{x, y};
        const auto found = currentIndex_.find(key);
        if (found == currentIndex_.end()) {
            const std::size_t index = current_.size();
            current_.push_back({x, y, beforeNow, alive});
            currentIndex_.emplace(key, index);
        } else {
            current_[found->second].after = alive;
        }

        board.setAlive(x, y, alive);
    }

    bool commit() {
        if (!active_) return false;
        active_ = false;

        std::erase_if(current_, [](const CellChange& change) {
            return change.before == change.after;
        });
        currentIndex_.clear();

        if (current_.empty()) {
            current_.clear();
            return false;
        }

        redo_.clear();
        undo_.push_back(Command{std::move(current_)});
        current_.clear();

        if (undo_.size() > maxCommands_) {
            undo_.erase(undo_.begin(), undo_.begin() + static_cast<std::ptrdiff_t>(undo_.size() - maxCommands_));
        }
        return true;
    }

    void discardActive() {
        active_ = false;
        current_.clear();
        currentIndex_.clear();
    }

    bool undo(InfiniteLifeBoard& board) {
        if (active_ || undo_.empty()) return false;

        Command command = std::move(undo_.back());
        undo_.pop_back();
        for (auto it = command.changes.rbegin(); it != command.changes.rend(); ++it) {
            board.setAlive(it->x, it->y, it->before);
        }
        redo_.push_back(std::move(command));
        return true;
    }

    bool redo(InfiniteLifeBoard& board) {
        if (active_ || redo_.empty()) return false;

        Command command = std::move(redo_.back());
        redo_.pop_back();
        for (const CellChange& change : command.changes) {
            board.setAlive(change.x, change.y, change.after);
        }
        undo_.push_back(std::move(command));
        return true;
    }

    void clear() {
        discardActive();
        undo_.clear();
        redo_.clear();
    }

    bool canUndo() const noexcept { return !undo_.empty(); }
    bool canRedo() const noexcept { return !redo_.empty(); }
    std::size_t undoCount() const noexcept { return undo_.size(); }
    std::size_t redoCount() const noexcept { return redo_.size(); }

private:
    struct CellKey {
        Coord x = 0;
        Coord y = 0;

        bool operator==(const CellKey&) const noexcept = default;
    };

    struct CellKeyHash {
        std::size_t operator()(const CellKey& key) const noexcept {
            const std::uint64_t x = static_cast<std::uint64_t>(key.x);
            const std::uint64_t y = static_cast<std::uint64_t>(key.y);
            const std::uint64_t mixed = x ^ (y + 0x9e3779b97f4a7c15ULL + (x << 6) + (x >> 2));
            return static_cast<std::size_t>(mixed ^ (mixed >> 32));
        }
    };

    struct CellChange {
        Coord x = 0;
        Coord y = 0;
        bool before = false;
        bool after = false;
    };

    struct Command {
        std::vector<CellChange> changes;
    };

    std::size_t maxCommands_ = 256;
    bool active_ = false;
    std::vector<CellChange> current_;
    std::unordered_map<CellKey, std::size_t, CellKeyHash> currentIndex_;
    std::vector<Command> undo_;
    std::vector<Command> redo_;
};
