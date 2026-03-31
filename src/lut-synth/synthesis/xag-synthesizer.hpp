#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <kitty/dynamic_truth_table.hpp>
#include <mockturtle/networks/xag.hpp>

namespace lut_synth { class TruthTable; }

namespace lut_synth {

/*! \brief Options for parameterized synthesis method creation. */
struct SynthesisOptions {
    uint32_t ac_max_lut = 4;          /*!< Max LUT size for AC method */
    int ss_k = -1;                     /*!< Shannon parameter for SS (-1 = auto) */
    uint32_t num_random_starts = 1;    /*!< Random starts for SS */
    uint64_t seed = 0;                 /*!< Random seed for SS */
    bool disable_dont_care = false;    /*!< Disable don't-care exploitation */
};

/*! \brief Abstract base class for XAG synthesis methods.
 *
 *  Provides a polymorphic interface for truth-table-to-XAG synthesis.
 *  Concrete subclasses wrap individual synthesis algorithms (davio, ac,
 *  dsd, ss, exact, rmdds) and self-register via a static registry so
 *  that methods can be selected by name at runtime.
 *
 *  Algorithm:
 *  1. Each subclass registers itself with a file-local static object
 *  2. Callers use create(name) to obtain the appropriate synthesizer
 *  3. The synthesizer is configured with method-specific parameters
 *  4. synthesize(tts) performs the actual synthesis
 *
 *  Example:
 *  ```cpp
 *  auto synth = XagSynthesizer::create("davio");
 *  auto xag = synth->synthesize(tts);
 *  ```
 */
class XagSynthesizer {
  public:
    virtual ~XagSynthesizer() = default;

    /*! \brief Return method name. */
    virtual std::string name() const = 0;

    /*! \brief Synthesize XAG from fully-specified truth tables. */
    virtual mockturtle::xag_network
    synthesize(std::vector<kitty::dynamic_truth_table> const &tts) const = 0;

    /*! \brief Synthesize XAG from TruthTable with possible don't-cares.
     *
     *  Default strips don't-cares and delegates to synthesize(tts).
     *  SSSynthesizer overrides to exploit don't-cares.
     */
    virtual mockturtle::xag_network synthesize(lut_synth::TruthTable const &tt) const;

    /*! \brief Whether this method exploits don't-care conditions. */
    virtual bool supports_dont_care() const { return false; }

    using Factory = std::function<std::unique_ptr<XagSynthesizer>()>;
    using ParameterizedFactory = std::function<std::unique_ptr<XagSynthesizer>(SynthesisOptions const&)>;

    /*! \brief Register a synthesis method factory.
     *  \param name Method name (lowercase)
     *  \param factory Callable returning a new synthesizer instance
     */
    static void register_method(std::string const &name, Factory factory);

    /*! \brief Register a parameterized synthesis method factory.
     *  \param name Method name (lowercase)
     *  \param factory Callable returning a new synthesizer from options
     */
    static void register_parameterized(std::string const &name, ParameterizedFactory factory);

    /*! \brief Create a synthesizer by method name.
     *  \param name Method name (lowercase)
     *  \return Synthesizer instance, or nullptr if name not found
     */
    static std::unique_ptr<XagSynthesizer> create(std::string const &name);

    /*! \brief Create a synthesizer by name with options.
     *  \param name Method name (lowercase)
     *  \param opts Synthesis options
     *  \return Synthesizer instance, or nullptr if name not found
     */
    static std::unique_ptr<XagSynthesizer> create(std::string const &name,
                                                    SynthesisOptions const &opts);

    /*! \brief Return all registered method names. */
    static std::vector<std::string> registered_methods();
};

} // namespace lut_synth
