#include "terrain_edit_history.h"
#include "terrain.h"

void TerrainEditHistory::Begin(const std::string &label, int layer) {
    if (pending_.has_value())
        End();
    TerrainEdit e;
    e.layer = layer;
    e.label = label;
    pending_ = std::move(e);
    pendingIndex_.clear();
}

void TerrainEditHistory::Record(int tx, int ty, int prev, int next) {
    if (!pending_)
        return;
    if (prev == next)
        return;
    const long long k = Key(tx, ty);
    auto it = pendingIndex_.find(k);
    if (it != pendingIndex_.end()) {
        // Already recorded this cell in the current stroke; update terminal value.
        pending_->cells[it->second].next = next;
        // If the cell ended up unchanged, drop it (rare but cheap to handle).
        if (pending_->cells[it->second].prev == next) {
            pending_->cells.erase(pending_->cells.begin() +
                                  static_cast<std::ptrdiff_t>(it->second));
            pendingIndex_.erase(it);
            // Rebuild index since vector indices shifted. For typical stroke
            // sizes this is fine; tracking ordering is not worth the bytes.
            pendingIndex_.clear();
            for (size_t i = 0; i < pending_->cells.size(); ++i) {
                pendingIndex_[Key(pending_->cells[i].tx, pending_->cells[i].ty)] = i;
            }
        }
        return;
    }
    pendingIndex_[k] = pending_->cells.size();
    pending_->cells.push_back({tx, ty, prev, next});
}

void TerrainEditHistory::End() {
    if (!pending_)
        return;
    if (!pending_->cells.empty()) {
        undo_.push_back(std::move(*pending_));
        if (undo_.size() > kMaxHistory)
            undo_.erase(undo_.begin());
        redo_.clear();
    }
    pending_.reset();
    pendingIndex_.clear();
}

void TerrainEditHistory::Undo(criogenio::Terrain2D &terrain) {
    if (undo_.empty())
        return;
    TerrainEdit e = std::move(undo_.back());
    undo_.pop_back();
    for (const auto &c : e.cells)
        terrain.SetTile(e.layer, c.tx, c.ty, c.prev);
    redo_.push_back(std::move(e));
}

void TerrainEditHistory::Redo(criogenio::Terrain2D &terrain) {
    if (redo_.empty())
        return;
    TerrainEdit e = std::move(redo_.back());
    redo_.pop_back();
    for (const auto &c : e.cells)
        terrain.SetTile(e.layer, c.tx, c.ty, c.next);
    undo_.push_back(std::move(e));
}

void TerrainEditHistory::Clear() {
    undo_.clear();
    redo_.clear();
    pending_.reset();
    pendingIndex_.clear();
}

std::string TerrainEditHistory::PeekUndoLabel() const {
    if (undo_.empty()) return {};
    return undo_.back().label;
}
std::string TerrainEditHistory::PeekRedoLabel() const {
    if (redo_.empty()) return {};
    return redo_.back().label;
}
