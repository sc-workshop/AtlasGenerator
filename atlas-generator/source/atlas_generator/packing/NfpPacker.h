#pragma once

#include "EdgeCache.h"
#include "NfpGeometry.h"
#include "PackingConfig.h"
#include "PackingTypes.h"

#include <limits>
#include <vector>

namespace wk::AtlasGenerator::packing {
    class NfpPacker {
    public:
        NfpPacker(const Box& bin, const PackingConfig& config);

        bool try_pack(PackingItem& item);
        void accept(const PackingItem& item);

    private:
        bool try_pack_serial(PackingItem& item,
                             double initial_rotation,
                             const Point& initial_translation,
                             const Box& pile_bbox);
        bool try_pack_parallel(PackingItem& item,
                               double initial_rotation,
                               const Point& initial_translation,
                               const Box& pile_bbox);

        struct Candidate {
            double score = std::numeric_limits<double>::max();
            double overfit = std::numeric_limits<double>::max();
            Point translation {0, 0};
            double rotation = 0.0;
        };

        struct Optimum {
            double position = 0.0;
            size_t nfp_index = 0;
            int hole_index = -1;
        };

        double score(const Box& candidate_bbox, const Box& pile_bbox, const Box& bin_bbox, double norm) const;
        double overfit(const Box& box, const Box& bin) const;
        void set_initial_position(PackingItem& item, const Box& bin_bbox) const;
        Shapes calculate_nfps(const PackingItem& item) const;
        Point nfp_point(const EdgeCache& cache, const Optimum& optimum) const;
        double boundary_check(const EdgeCache& cache,
                              const Optimum& optimum,
                              const Point& nearest_vertex,
                              const Point& start_position,
                              const Box& local_bbox) const;

        Box m_bin;
        Box m_bin_bbox;
        PackingConfig m_config;
        double m_norm;
        Box m_merged_bbox;
        bool m_has_merged_bbox = false;
        std::vector<PackingItem> m_items;
    };
}
