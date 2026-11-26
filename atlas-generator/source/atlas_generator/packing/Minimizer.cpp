#include "Minimizer.h"

#include <cmath>
#include <exception>
#include <nlopt.hpp>
#include <string>
#include <vector>

namespace wk::AtlasGenerator::packing {
    namespace {
        struct CallbackData {
            const std::function<double(double)>* function;
        };

        double objective(unsigned, const double* position, double*, void* data) {
            const auto* callback = static_cast<CallbackData*>(data);
            return (*callback->function)(position[0]);
        }
    }

    Minimizer::Minimizer(double accuracy) :
        m_max_iterations(static_cast<unsigned>(std::floor(1000.0 * accuracy))) {
    }

    MinimizationResult Minimizer::minimize(const std::function<double(double)>& function, double initial_position) {
        CallbackData data {&function};

        nlopt::opt solver(nlopt::LN_SBPLX, 1);
        solver.set_lower_bounds(std::vector<double> {0.0});
        solver.set_upper_bounds(std::vector<double> {1.0});
        solver.set_ftol_rel(1e-12);
        solver.set_maxeval(m_max_iterations);
        solver.set_min_objective(objective, &data);

        std::vector<double> position {initial_position};
        double value = 0.0;
        try {
            solver.optimize(position, value);
        } catch (const nlopt::forced_stop&) {
            // Keep the best point and score NLopt already wrote.
        } catch (const nlopt::roundoff_limited&) {
            // Baseline patch keeps the best point and score.
        } catch (const std::exception& error) {
            const std::string message = error.what();
            const bool roundoff = message.find("roundoff") != std::string::npos;
            const bool forced_stop = message.find("forced stop") != std::string::npos;
            if (!roundoff && !forced_stop)
                throw;
        }

        return {value, position[0]};
    }
}
