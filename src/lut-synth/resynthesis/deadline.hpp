#pragma once

#include <chrono>

namespace lut_synth {

using resynth_clock = std::chrono::high_resolution_clock;

/*! \brief Value type encapsulating a time budget with a start point.
 *
 *  Replaces scattered (start, timeout_s) pairs and the free functions
 *  is_timed_out(), elapsed_ms(), remaining_s().
 */
class Deadline {
public:
    /*! \brief Create a deadline starting now. */
    explicit Deadline(double timeout_s)
        : start_(resynth_clock::now()), timeout_s_(timeout_s) {}

    /*! \brief Check if the deadline has passed. */
    bool expired() const {
        if (timeout_s_ <= 0.0) return false;
        double elapsed = std::chrono::duration<double>(resynth_clock::now() - start_).count();
        return elapsed >= timeout_s_;
    }

    /*! \brief Milliseconds elapsed since start. */
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(resynth_clock::now() - start_).count();
    }

    /*! \brief Seconds remaining until deadline. */
    double remaining_s() const {
        double elapsed = std::chrono::duration<double>(resynth_clock::now() - start_).count();
        return timeout_s_ - elapsed;
    }

    /*! \brief Create a sub-deadline using a fraction of the total budget.
     *  \param ratio Fraction of timeout_s (e.g. 0.40 for 40%)
     *  \return Deadline sharing the same start but with timeout_s * ratio
     */
    Deadline fraction(double ratio) const {
        return Deadline(start_, timeout_s_ * ratio);
    }

    /*! \brief Create an infinite deadline (never expires). */
    static Deadline none() { return Deadline(0.0); }

private:
    Deadline(resynth_clock::time_point start, double timeout_s)
        : start_(start), timeout_s_(timeout_s) {}

    resynth_clock::time_point start_;
    double timeout_s_;
};

} // namespace lut_synth
