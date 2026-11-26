#pragma once

#include <functional>

namespace wk::AtlasGenerator::packing {
    struct MinimizationResult {
        double value = 0.0;
        double position = 0.0;
    };

    class Minimizer final {
    public:
        explicit Minimizer(double accuracy);

        MinimizationResult minimize(const std::function<double(double)>& function, double initial_position);

    private:
        unsigned m_max_iterations;
    };
}
