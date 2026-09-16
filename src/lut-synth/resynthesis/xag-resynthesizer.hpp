#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <mockturtle/networks/xag.hpp>

namespace lut_synth {

struct ResynthesisResult;

/*! \brief Abstract base class for XAG resynthesis methods.
 *
 *  Provides a polymorphic interface for XAG network resynthesis.
 *  Concrete subclasses wrap individual resynthesis algorithms (anysyn,
 *  v5, v7) and are centrally registered so that methods can be selected
 *  by name at runtime.
 *
 *  Algorithm:
 *  1. Each subclass is registered in xag-resynthesizer.cpp
 *  2. Callers use create(name) to obtain the appropriate resynthesizer
 *  3. set_timeout() adjusts the time budget before running
 *  4. resynthesize(xag) performs the actual optimization
 *
 *  Example:
 *  ```cpp
 *  auto resyn = XagResynthesizer::create("v7");
 *  resyn->set_timeout(60.0);
 *  auto optimized = resyn->resynthesize(xag);
 *  ```
 */
class XagResynthesizer {
  public:
    virtual ~XagResynthesizer() = default;

    /*! \brief Return method name. */
    virtual std::string name() const = 0;

    /*! \brief Resynthesize XAG to minimize multiplicative complexity.
     *
     *  Default delegates to resynthesize_with_report() and returns the
     *  network only.
     */
    virtual mockturtle::xag_network
    resynthesize(mockturtle::xag_network const &xag) const;

    /*! \brief Resynthesize XAG and return report data. */
    virtual ResynthesisResult
    resynthesize_with_report(mockturtle::xag_network const &xag) const = 0;

    /*! \brief Override the version's default timeout. */
    virtual void set_timeout(double timeout_s) = 0;

    using Factory = std::function<std::unique_ptr<XagResynthesizer>()>;

    /*! \brief Register a resynthesis method factory.
     *  \param name Method name (lowercase)
     *  \param factory Callable returning a new resynthesizer instance
     */
    static void register_method(std::string const &name, Factory factory);

    /*! \brief Create a resynthesizer by method name.
     *  \param name Method name (lowercase)
     *  \return Resynthesizer instance, or nullptr if name not found
     */
    static std::unique_ptr<XagResynthesizer> create(std::string const &name);

    /*! \brief Return all registered method names. */
    static std::vector<std::string> registered_methods();
};

} // namespace lut_synth
