#pragma once

#include <algorithm>
#include <functional>
#include <stdint.h>

namespace wk::AtlasGenerator {
    // Selects how items are ordered before packing.
    enum class PackingPriority {
        // Area-based ordering. Minimizes generation time and is the default.
        Speed,
        // Max-side ordering. Can reduce atlas area at extra runtime cost.
        Density
    };

    // Runtime settings for atlas generation
    class Config {
    public:
        Config(uint16_t width,
               uint16_t height,
               float scale,
               uint8_t extrude,
               bool parallel = true,
               uint8_t alpha_threshold = 3,
               PackingPriority packing_priority = PackingPriority::Speed);
        virtual ~Config() = default;

    public:
        // Maximum atlas texture width in pixels. Out-of-range input is clamped.
        virtual uint16_t width() const { return m_max_width; }

        // Maximum atlas texture height in pixels. Out-of-range input is clamped.
        virtual uint16_t height() const { return m_max_height; }

        // Scale factor applied before polygon generation. Out-of-range input is clamped.
        virtual float scale() const { return m_scale_factor; }

        // Pixel padding around each sprite. The packer uses twice this value as
        // the minimum distance between items.
        virtual uint8_t extrude() const { return m_extrude; }

        // Enables internal parallel packing. Disable to limit CPU use or isolate
        // threading issues.
        virtual bool parallel() const { return m_parallel; }

        // Alpha values greater than this threshold are treated as opaque. Raising
        // it discards more near-transparent pixels.
        virtual uint8_t alpha_threshold() const { return m_alpha_threshold; }

        // Selects the ordering strategy used by the packer.
        virtual PackingPriority packing_priority() const { return m_packing_priority; }

    private:
        const uint16_t m_max_width;
        const uint16_t m_max_height;
        const float m_scale_factor;
        const uint8_t m_extrude;
        const bool m_parallel;
        const uint8_t m_alpha_threshold;
        const PackingPriority m_packing_priority;

    public:
        // Optional progress callback. Receives the number of processed items.
        // May be empty when the caller does not need progress reporting.
        std::function<void(size_t)> progress;
    };
}
