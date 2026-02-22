#pragma once
#include <string>
#include <vector>
#include <cstdint>

/**
 * ArabicUtils: UTF-8 aware utilities for Arabic morphological processing.
 *
 * Arabic root-pattern morphology works as follows:
 *   - A trilateral root has 3 consonants, e.g., ك-ت-ب
 *   - A morphological pattern (schème) is a template using ف, ع, ل as placeholders
 *     ف = first radical (fa), ع = second radical (ain), ل = third radical (lam)
 *   - Generation: replace placeholders with root consonants
 *   - Validation: extract consonants and match to root
 *
 * Codepoints used as pattern placeholder markers:
 *   U+0641 ف  = first radical position
 *   U+0639 ع  = second radical position
 *   U+0644 ل  = third radical position
 */
namespace ArabicUtils {

    // Placeholder codepoints in pattern templates
    static const uint32_t FA  = 0x0641u; // ف  first  radical
    static const uint32_t AIN = 0x0639u; // ع  second radical
    static const uint32_t LAM = 0x0644u; // ل  third  radical

    // Arabic Unicode block ranges
    static const uint32_t ARABIC_START = 0x0600u;
    static const uint32_t ARABIC_END   = 0x06FFu;

    // Diacritic (harakat) codepoints – not consonants
    static const uint32_t FATHAH      = 0x064Eu;
    static const uint32_t DAMMAH      = 0x064Fu;
    static const uint32_t KASRAH      = 0x0650u;
    static const uint32_t SUKUN       = 0x0652u;
    static const uint32_t SHADDA      = 0x0651u;
    static const uint32_t TANWIN_FAT  = 0x064Bu;
    static const uint32_t TANWIN_DAM  = 0x064Cu;
    static const uint32_t TANWIN_KAS  = 0x064Du;
    static const uint32_t TATWEEL     = 0x0640u;

    /**
     * Decode a UTF-8 string into a vector of Unicode codepoints.
     */
    std::vector<uint32_t> toCodepoints(const std::string& utf8);

    /**
     * Encode a vector of codepoints back to UTF-8.
     */
    std::string fromCodepoints(const std::vector<uint32_t>& codepoints);

    /**
     * Encode a single codepoint to UTF-8 bytes.
     */
    std::string cpToUtf8(uint32_t cp);

    /**
     * Returns true if cp is any Arabic letter (U+0600–U+06FF, excluding diacritics).
     */
    bool isArabicConsonant(uint32_t cp);

    /**
     * Returns true if cp is a harakat (diacritic mark).
     */
    bool isDiacritic(uint32_t cp);

    /**
     * Strip all diacritical marks from an Arabic UTF-8 string.
     */
    std::string stripDiacritics(const std::string& word);

    /**
     * Extract only consonant codepoints from a word (strips diacritics and tatweel).
     */
    std::vector<uint32_t> extractConsonants(const std::string& word);

    /**
     * Lexicographic comparison of two Arabic UTF-8 strings by codepoint value.
     * Returns < 0, 0, or > 0.
     */
    int compare(const std::string& a, const std::string& b);

    /**
     * Apply a pattern to a root:
     *   root  : vector of 3 consonant codepoints [r0, r1, r2]
     *   pattern : pattern name as UTF-8 string (e.g. "فاعل")
     * Returns the derived word as UTF-8, or "" on error.
     */
    std::string applyPattern(const std::vector<uint32_t>& root,
                             const std::string& patternTemplate);

    /**
     * Try to extract the root from a word given a pattern template.
     * Returns the extracted consonants or an empty vector if the word
     * does not match the pattern shape.
     */
    std::vector<uint32_t> extractRoot(const std::string& word,
                                      const std::string& patternTemplate);

    /**
     * Returns true if `word` was derived from `root` under `patternTemplate`.
     */
    bool matchesPattern(const std::string& word,
                        const std::vector<uint32_t>& root,
                        const std::string& patternTemplate);

    /**
     * Count number of radicals (ف/ع/ل occurrences) in a pattern.
     */
    int countRadicals(const std::string& patternTemplate);

} // namespace ArabicUtils
