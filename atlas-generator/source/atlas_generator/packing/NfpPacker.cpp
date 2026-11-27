#include "NfpPacker.h"

#include "Minimizer.h"
#include "ParallelFor.h"
#include "RotationSelector.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <memory>
#include <utility>

namespace wk::AtlasGenerator::packing {
    namespace {
        Minimizer& thread_minimizer(const PackingConfig& config) {
            struct State {
                double accuracy = -1.0;
                std::unique_ptr<Minimizer> minimizer;
            };
            thread_local State state;
            if (!state.minimizer || state.accuracy != config.accuracy) {
                state.minimizer = std::make_unique<Minimizer>(config.accuracy);
                state.accuracy = config.accuracy;
            }
            return *state.minimizer;
        }

        struct StartTask {
            size_t cache_index = 0;
            int hole_index = -1;
            double start = 0.0;
        };

        struct StartGroup {
            size_t begin = 0;
            size_t end = 0;
            size_t cache_index = 0;
            int hole_index = -1;
        };

        struct RotationContext {
            PackingItem current;
            Box rotated_bbox;
            double lower_bound = 0.0;
            Point nearest_vertex;
            Point start_position;
            Box local_bbox;
            Shapes nfps;
            bool merged_valid = false;
            std::vector<EdgeCache> caches;
            std::vector<StartGroup> groups;
            size_t task_begin = 0;
            size_t task_end = 0;
        };

    }

    NfpPacker::NfpPacker(const Box& bin, const PackingConfig& config) :
        m_bin(bin),
        m_bin_bbox(bin),
        m_config(config),
        m_norm(std::sqrt(m_bin.area())) {
    }

    double NfpPacker::score(const Box& candidate_bbox, const Box& pile_bbox, const Box& bin_bbox, double norm) const {
        const Box full = unite(pile_bbox, candidate_bbox);
        double result = static_cast<double>(full.width() + full.height()) / norm;
        if (!contains(bin_bbox, full))
            result += norm;
        return result;
    }

    double NfpPacker::overfit(const Box& box, const Box& bin) const {
        const double width_diff = static_cast<double>(box.width()) - static_cast<double>(bin.width());
        const double height_diff = static_cast<double>(box.height()) - static_cast<double>(bin.height());
        double result = 0.0;
        if (width_diff > 0)
            result += width_diff;
        if (height_diff > 0)
            result += height_diff;
        return result;
    }

    void NfpPacker::set_initial_position(PackingItem& item, const Box& bin_bbox) const {
        item.prepare();
        const Box box = item.bounding_box();
        item.translate({bin_bbox.min.X - box.min.X, bin_bbox.min.Y - box.min.Y});
    }

    Shapes NfpPacker::calculate_nfps(const PackingItem& item) const {
        const size_t count = m_items.size();
        Shapes nfps(count);
        const Path& orbiter = item.transformed_shape().contour;

        const auto build_nfp = [&](size_t index) {
            const PackingItem& placed = m_items[index];
            NfpResult nfp = convex_nfp(placed.transformed_shape().contour, orbiter);
            correct_nfp_position(nfp, placed, item);
            nfps[index].contour = std::move(nfp.path);
        };

        if (m_config.parallel && count >= 8) {
            parallel_for_indexed(count, build_nfp);
        } else {
            for (size_t index = 0; index < count; ++index)
                build_nfp(index);
        }

        return merge_shapes(nfps);
    }

    Point NfpPacker::nfp_point(const EdgeCache& cache, const Optimum& optimum) const {
        return optimum.hole_index < 0 ? cache.coords(optimum.position)
                                      : cache.coords(static_cast<size_t>(optimum.hole_index), optimum.position);
    }

    double NfpPacker::boundary_check(const EdgeCache& cache,
                                     const Optimum& optimum,
                                     const Point& nearest_vertex,
                                     const Point& start_position,
                                     const Box& local_bbox) const {
        const Point point = nfp_point(cache, optimum);
        const Point translation {point.X - nearest_vertex.X + start_position.X,
                                 point.Y - nearest_vertex.Y + start_position.Y};
        const Box placed_bbox {local_bbox.min + translation, local_bbox.max + translation};

        return contains(m_bin_bbox, unite(m_merged_bbox, placed_bbox)) ? -1.0 : 1.0;
    }

    bool NfpPacker::try_pack(PackingItem& item) {
        const double initial_rotation = item.rotation();
        const Point initial_translation = item.translation();
        const Box pile_bbox = m_merged_bbox;

        bool packed = false;
        if (m_items.empty()) {
            set_initial_position(item, m_bin_bbox);
            item.prepare();
            Point best_translation = item.translation();
            double best_rotation = item.rotation();
            double best_overfit = overfit(bounding_box(item.transformed_shape()), m_bin_bbox);

            for (const double rotation : default_rotations()) {
                item.translation(initial_translation);
                item.rotation(initial_rotation + rotation);
                set_initial_position(item, m_bin_bbox);
                item.prepare();

                const double current_overfit = overfit(bounding_box(item.transformed_shape()), m_bin_bbox);
                if (current_overfit < best_overfit) {
                    best_overfit = current_overfit;
                    best_translation = item.translation();
                    best_rotation = item.rotation();
                }
            }

            item.rotation(best_rotation);
            item.translation(best_translation);
            packed = best_overfit <= 0.0;
        } else if (m_config.parallel) {
            packed = try_pack_parallel(item, initial_rotation, initial_translation, pile_bbox);
        } else {
            packed = try_pack_serial(item, initial_rotation, initial_translation, pile_bbox);
        }

        item.prepare();
        return packed;
    }

    bool NfpPacker::try_pack_serial(PackingItem& item,
                                    double initial_rotation,
                                    const Point& initial_translation,
                                    const Box& pile_bbox) {
        const std::vector<double> rotations = optimized_rotations(item);
        std::vector<Candidate> candidates(rotations.size());
        double best_overfit = std::numeric_limits<double>::max();
        double best_score = std::numeric_limits<double>::max();

        for (size_t rotation_index = 0; rotation_index < rotations.size(); ++rotation_index) {
            PackingItem current = item;
            current.translation(initial_translation);
            current.rotation(initial_rotation + rotations[rotation_index]);
            current.prepare();
            const Box rotated_bbox = current.bounding_box();

            const Coord lower_width = std::max(pile_bbox.width(), rotated_bbox.width());
            const Coord lower_height = std::max(pile_bbox.height(), rotated_bbox.height());
            const double lower_bound = static_cast<double>(lower_width + lower_height) / m_norm;
            if (best_score <= lower_bound)
                continue;

            const Shapes nfps = calculate_nfps(current);
            if (nfps.empty())
                continue;

            const Point nearest_vertex = current.reference_vertex();
            const Point start_position = current.translation();
            const Box local_bbox {rotated_bbox.min - start_position, rotated_bbox.max - start_position};

            std::vector<EdgeCache> caches;
            caches.reserve(nfps.size());
            for (const Polygon& nfp : nfps) {
                caches.emplace_back(nfp, m_config.accuracy);
            }

            std::vector<StartTask> tasks;
            std::vector<StartGroup> groups;
            for (size_t cache_index = 0; cache_index < caches.size(); ++cache_index) {
                {
                    const size_t begin = tasks.size();
                    for (const double start : caches[cache_index].corners()) {
                        tasks.push_back({cache_index, -1, start});
                    }
                    groups.push_back({begin, tasks.size(), cache_index, -1});
                }

                for (size_t hole_index = 0; hole_index < caches[cache_index].hole_count(); ++hole_index) {
                    const size_t begin = tasks.size();
                    for (const double start : caches[cache_index].corners(hole_index)) {
                        tasks.push_back({cache_index, static_cast<int>(hole_index), start});
                    }
                    groups.push_back({begin, tasks.size(), cache_index, static_cast<int>(hole_index)});
                }
            }

            std::vector<MinimizationResult> results(tasks.size());
            const auto optimize_task = [&](size_t task_index) {
                const StartTask& task = tasks[task_index];
                const EdgeCache& cache = caches[task.cache_index];

                const auto objective = [&](double position) {
                    const Point point = task.hole_index < 0
                                            ? cache.coords(position)
                                            : cache.coords(static_cast<size_t>(task.hole_index), position);
                    const Point translation {point.X - nearest_vertex.X + start_position.X,
                                             point.Y - nearest_vertex.Y + start_position.Y};
                    const Box placed_bbox {local_bbox.min + translation, local_bbox.max + translation};
                    return score(placed_bbox, pile_bbox, m_bin_bbox, m_norm);
                };

                try {
                    results[task_index] = thread_minimizer(m_config).minimize(objective, task.start);
                } catch (const std::exception&) {
                    // Default-constructed score stays 0.0.
                }
            };

            for (size_t task_index = 0; task_index < tasks.size(); ++task_index) {
                optimize_task(task_index);
            }

            Optimum optimum;
            double local_overfit = std::numeric_limits<double>::max();

            const auto accept = [&](const StartGroup& group) {
                if (group.begin == group.end)
                    return;

                MinimizationResult minimal = results[group.begin];
                for (size_t result_index = group.begin + 1; result_index < group.end; ++result_index) {
                    if (results[result_index].value < minimal.value) {
                        minimal = results[result_index];
                    }
                }
                if (minimal.value >= best_score)
                    return;

                const Optimum candidate_optimum {minimal.position, group.cache_index, group.hole_index};
                const double miss = boundary_check(caches[group.cache_index],
                                                   candidate_optimum,
                                                   nearest_vertex,
                                                   start_position,
                                                   local_bbox);

                if (miss <= 0.0) {
                    best_score = minimal.value;
                    optimum = candidate_optimum;
                } else {
                    local_overfit = std::min(miss, local_overfit);
                }
            };

            for (const StartGroup& group : groups)
                accept(group);

            Candidate& candidate = candidates[rotation_index];
            candidate.score = best_score;
            candidate.overfit = local_overfit;
            candidate.rotation = initial_rotation + rotations[rotation_index];
            if (best_score < std::numeric_limits<double>::max()) {
                const Point point = nfp_point(caches[optimum.nfp_index], optimum);
                candidate.translation = {point.X - nearest_vertex.X + start_position.X,
                                         point.Y - nearest_vertex.Y + start_position.Y};
            }
        }

        const auto best_candidate =
            std::min_element(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                return a.score < b.score;
            });
        if (best_candidate == candidates.end() || best_candidate->score >= std::numeric_limits<double>::max()) {
            const auto worst =
                std::min_element(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                    return a.overfit < b.overfit;
                });
            if (worst != candidates.end())
                best_overfit = worst->overfit;
            return false;
        }

        item.rotation(best_candidate->rotation);
        item.translation(best_candidate->translation);
        return true;
    }

    bool NfpPacker::try_pack_parallel(PackingItem& item,
                                      double initial_rotation,
                                      const Point& initial_translation,
                                      const Box& pile_bbox) {
        const std::vector<double> rotations = optimized_rotations(item);
        std::vector<Candidate> candidates(rotations.size());
        double best_overfit = std::numeric_limits<double>::max();
        double best_score = std::numeric_limits<double>::max();

        std::vector<RotationContext> contexts;
        contexts.reserve(rotations.size());
        for (const double rotation : rotations) {
            RotationContext context;
            context.current = item;
            context.current.translation(initial_translation);
            context.current.rotation(initial_rotation + rotation);
            context.current.prepare();
            context.rotated_bbox = context.current.bounding_box();

            const Coord lower_width = std::max(pile_bbox.width(), context.rotated_bbox.width());
            const Coord lower_height = std::max(pile_bbox.height(), context.rotated_bbox.height());
            context.lower_bound = static_cast<double>(lower_width + lower_height) / m_norm;
            context.nearest_vertex = context.current.reference_vertex();
            context.start_position = context.current.translation();
            context.local_bbox = {context.rotated_bbox.min - context.start_position,
                                  context.rotated_bbox.max - context.start_position};
            context.nfps.resize(m_items.size());
            contexts.push_back(std::move(context));
        }

        const size_t placed_count = m_items.size();
        const size_t nfp_count = rotations.size() * placed_count;

        // Every PackingItem geometry cache is prepared before workers start; accessors are read-only.
        const auto build_all_nfps = [&](size_t linear_index) {
            const size_t rotation_index = linear_index / placed_count;
            const size_t placed_index = linear_index % placed_count;
            const RotationContext& context = contexts[rotation_index];
            const PackingItem& placed = m_items[placed_index];
            const Path& orbiter = context.current.transformed_shape().contour;

            NfpResult nfp = convex_nfp(placed.transformed_shape().contour, orbiter);
            correct_nfp_position(nfp, placed, context.current);
            contexts[rotation_index].nfps[placed_index].contour = std::move(nfp.path);
        };

        if (nfp_count >= 8) {
            parallel_for_indexed(nfp_count, build_all_nfps);
        } else {
            for (size_t index = 0; index < nfp_count; ++index)
                build_all_nfps(index);
        }

        const auto prepare_rotation = [&](size_t rotation_index) {
            RotationContext& context = contexts[rotation_index];
            const Shapes merged = merge_shapes(context.nfps);
            context.nfps.clear();
            if (merged.empty())
                return;

            context.merged_valid = true;
            context.caches.reserve(merged.size());
            for (const Polygon& nfp : merged) {
                context.caches.emplace_back(nfp, m_config.accuracy);
            }
        };

        if (rotations.size() >= 2) {
            parallel_for_indexed(rotations.size(), prepare_rotation);
        } else {
            prepare_rotation(0);
        }

        std::vector<StartTask> tasks;
        std::vector<size_t> task_owner;
        for (size_t rotation_index = 0; rotation_index < contexts.size(); ++rotation_index) {
            RotationContext& context = contexts[rotation_index];
            if (!context.merged_valid)
                continue;

            context.task_begin = tasks.size();
            for (size_t cache_index = 0; cache_index < context.caches.size(); ++cache_index) {
                {
                    const size_t begin = tasks.size();
                    for (const double start : context.caches[cache_index].corners()) {
                        tasks.push_back({cache_index, -1, start});
                        task_owner.push_back(rotation_index);
                    }
                    context.groups.push_back({begin, tasks.size(), cache_index, -1});
                }

                for (size_t hole_index = 0; hole_index < context.caches[cache_index].hole_count(); ++hole_index) {
                    const size_t begin = tasks.size();
                    for (const double start : context.caches[cache_index].corners(hole_index)) {
                        tasks.push_back({cache_index, static_cast<int>(hole_index), start});
                        task_owner.push_back(rotation_index);
                    }
                    context.groups.push_back({begin, tasks.size(), cache_index, static_cast<int>(hole_index)});
                }
            }
            context.task_end = tasks.size();
        }

        std::vector<MinimizationResult> results(tasks.size());

        const auto optimize_task = [&](size_t task_index) {
            const StartTask& task = tasks[task_index];
            const RotationContext& context = contexts[task_owner[task_index]];
            const EdgeCache& cache = context.caches[task.cache_index];

            const auto objective = [&](double position) {
                const Point point = task.hole_index < 0 ? cache.coords(position)
                                                        : cache.coords(static_cast<size_t>(task.hole_index), position);
                const Point translation {point.X - context.nearest_vertex.X + context.start_position.X,
                                         point.Y - context.nearest_vertex.Y + context.start_position.Y};
                const Box placed_bbox {context.local_bbox.min + translation, context.local_bbox.max + translation};
                return score(placed_bbox, pile_bbox, m_bin_bbox, m_norm);
            };

            try {
                results[task_index] = thread_minimizer(m_config).minimize(objective, task.start);
            } catch (const std::exception&) {
                // Default-constructed score stays 0.0.
            }
        };

        parallel_for_indexed(tasks.size(), optimize_task);

        for (size_t rotation_index = 0; rotation_index < rotations.size(); ++rotation_index) {
            const RotationContext& context = contexts[rotation_index];
            if (best_score <= context.lower_bound)
                continue;
            if (!context.merged_valid)
                continue;

            Optimum optimum;
            double local_overfit = std::numeric_limits<double>::max();

            const auto accept = [&](const StartGroup& group) {
                if (group.begin == group.end)
                    return;

                MinimizationResult minimal = results[group.begin];
                for (size_t result_index = group.begin + 1; result_index < group.end; ++result_index) {
                    if (results[result_index].value < minimal.value) {
                        minimal = results[result_index];
                    }
                }
                if (minimal.value >= best_score)
                    return;

                const Optimum candidate_optimum {minimal.position, group.cache_index, group.hole_index};
                const double miss = boundary_check(context.caches[group.cache_index],
                                                   candidate_optimum,
                                                   context.nearest_vertex,
                                                   context.start_position,
                                                   context.local_bbox);

                if (miss <= 0.0) {
                    best_score = minimal.value;
                    optimum = candidate_optimum;
                } else {
                    local_overfit = std::min(miss, local_overfit);
                }
            };

            for (const StartGroup& group : context.groups)
                accept(group);

            Candidate& candidate = candidates[rotation_index];
            candidate.score = best_score;
            candidate.overfit = local_overfit;
            candidate.rotation = initial_rotation + rotations[rotation_index];
            if (best_score < std::numeric_limits<double>::max()) {
                const Point point = nfp_point(context.caches[optimum.nfp_index], optimum);
                candidate.translation = {point.X - context.nearest_vertex.X + context.start_position.X,
                                         point.Y - context.nearest_vertex.Y + context.start_position.Y};
            }
        }

        const auto best_candidate =
            std::min_element(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                return a.score < b.score;
            });
        if (best_candidate == candidates.end() || best_candidate->score >= std::numeric_limits<double>::max()) {
            const auto worst =
                std::min_element(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
                    return a.overfit < b.overfit;
                });
            if (worst != candidates.end())
                best_overfit = worst->overfit;
            return false;
        }

        item.rotation(best_candidate->rotation);
        item.translation(best_candidate->translation);
        return true;
    }

    void NfpPacker::accept(const PackingItem& item) {
        // try_pack() leaves the item prepared; copying keeps the immutable caches for parallel NFP reads.
        m_items.emplace_back(item);
        const Box item_bbox = bounding_box(item.transformed_shape());
        m_merged_bbox = m_has_merged_bbox ? unite(m_merged_bbox, item_bbox) : item_bbox;
        m_has_merged_bbox = true;
    }
}
