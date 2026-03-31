#pragma once

#include <string>
#include <vector>

namespace lut_synth::util {

/*! \brief Trim whitespace from both ends of string.
 *  \param s Input string
 *  \return Trimmed string
 */
std::string trim(std::string const &s);

/*! \brief Trim whitespace in-place.
 *  \param s String to trim (modified)
 */
void trim_inplace(std::string &s);

/*! \brief Split CSV line and trim each token.
 *  \param line CSV line
 *  \return Vector of trimmed tokens
 */
std::vector<std::string> split_csv(std::string const &line);

/*! \brief Split string by delimiter.
 *  \param s Input string
 *  \param delim Delimiter character
 *  \return Vector of split tokens (not trimmed)
 */
std::vector<std::string> split(std::string const &s, char delim);

/*! \brief Split by commas, respecting nested parentheses.
 *  \param s Input string
 *  \return Vector of trimmed tokens
 */
std::vector<std::string> split_top_level_commas(std::string const &s);

} // namespace lut_synth::util
