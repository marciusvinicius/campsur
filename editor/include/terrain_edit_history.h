#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace criogenio {
class Terrain2D;
}

/**
 * Per-stroke undo/redo for tile painting.
 *
 * Lifecycle:
 *   history.Begin("Paint", layer);
 *   for each cell painted: history.Record(tx, ty, prev, next);
 *   history.End(terrain);   // pushes the stroke onto the undo stack
 *
 * The Record() call dedups inside a single stroke: painting (5,5)
 * twice in the same stroke only stores the FIRST prev value but the
 * LAST next value, so undo restores the pre-stroke state.
 */
struct TerrainEdit {
    int layer = 0;
    std::string label;
    struct CellDelta { int tx, ty, prev, next; };
    std::vector<CellDelta> cells;
};

class TerrainEditHistory {
public:
    /** Begin a new stroke. End() any prior open stroke first. */
    void Begin(const std::string &label, int layer);
    bool IsRecording() const { return pending_.has_value(); }
    /**
     * Record a cell mutation. First call for a given (tx, ty) keeps `prev`,
     * subsequent calls only update `next`. No-ops (prev == next) are skipped.
     */
    void Record(int tx, int ty, int prev, int next);
    /** Close the current stroke. Pushes onto the undo stack if non-empty. */
    void End();

    bool CanUndo() const { return !undo_.empty(); }
    bool CanRedo() const { return !redo_.empty(); }
    void Undo(criogenio::Terrain2D &terrain);
    void Redo(criogenio::Terrain2D &terrain);
    void Clear();

    size_t UndoSize() const { return undo_.size(); }
    size_t RedoSize() const { return redo_.size(); }
    /** Label of the next undoable stroke (empty if none). */
    std::string PeekUndoLabel() const;
    std::string PeekRedoLabel() const;

private:
    static constexpr size_t kMaxHistory = 128;
    std::vector<TerrainEdit> undo_;
    std::vector<TerrainEdit> redo_;
    std::optional<TerrainEdit> pending_;
    /** Tracks first prev per (tx, ty) within the active stroke for dedup. */
    std::unordered_map<long long, size_t> pendingIndex_;

    static long long Key(int tx, int ty) {
        return (static_cast<long long>(tx) << 32) ^ static_cast<unsigned int>(ty);
    }
};
