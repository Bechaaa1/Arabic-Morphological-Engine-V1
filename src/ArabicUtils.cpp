#include "ArabicUtils.h"
#include <stdexcept>
#include <algorithm>
#include <set>

namespace ArabicUtils {

// ─────────────────────────────────────────────────────────────────────────────
//  UTF-8 codec
// ─────────────────────────────────────────────────────────────────────────────

std::vector<uint32_t> toCodepoints(const std::string& utf8) {
    std::vector<uint32_t> result;
    size_t i = 0;
    const unsigned char* s = reinterpret_cast<const unsigned char*>(utf8.c_str());
    size_t len = utf8.size();

    while (i < len) {
        uint32_t cp = 0;
        unsigned char c = s[i];

        if ((c & 0x80) == 0x00) {           // 1-byte: 0xxxxxxx
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {    // 2-byte: 110xxxxx 10xxxxxx
            if (i + 1 >= len) break;
            cp = (c & 0x1F);
            cp = (cp << 6) | (s[i+1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {    // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 >= len) break;
            cp = (c & 0x0F);
            cp = (cp << 6) | (s[i+1] & 0x3F);
            cp = (cp << 6) | (s[i+2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {    // 4-byte: 11110xxx ...
            if (i + 3 >= len) break;
            cp = (c & 0x07);
            cp = (cp << 6) | (s[i+1] & 0x3F);
            cp = (cp << 6) | (s[i+2] & 0x3F);
            cp = (cp << 6) | (s[i+3] & 0x3F);
            i += 4;
        } else {
            i++; // skip invalid byte
            continue;
        }
        result.push_back(cp);
    }
    return result;
}

std::string cpToUtf8(uint32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

std::string fromCodepoints(const std::vector<uint32_t>& codepoints) {
    std::string result;
    for (uint32_t cp : codepoints) {
        result += cpToUtf8(cp);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Arabic character classification
// ─────────────────────────────────────────────────────────────────────────────

bool isDiacritic(uint32_t cp) {
    // Harakat and other non-letter Arabic marks
    return (cp == FATHAH   || cp == DAMMAH   || cp == KASRAH  ||
            cp == SUKUN    || cp == SHADDA    ||
            cp == TANWIN_FAT || cp == TANWIN_DAM || cp == TANWIN_KAS ||
            cp == TATWEEL  ||
            (cp >= 0x0610 && cp <= 0x061A) || // extended Arabic marks
            (cp >= 0x064B && cp <= 0x065F));   // combining marks block
}

bool isArabicConsonant(uint32_t cp) {
    // Main Arabic letter block (U+0621–U+063A, U+0641–U+064A)
    // plus extended (U+0671+)
    return (cp >= 0x0621 && cp <= 0x063A) ||
           (cp >= 0x0641 && cp <= 0x064A) ||
           (cp >= 0x0671 && cp <= 0x06D3);
}

// ─────────────────────────────────────────────────────────────────────────────
//  String utilities
// ─────────────────────────────────────────────────────────────────────────────

std::string stripDiacritics(const std::string& word) {
    auto cps = toCodepoints(word);
    std::vector<uint32_t> out;
    out.reserve(cps.size());
    for (uint32_t cp : cps) {
        if (!isDiacritic(cp)) out.push_back(cp);
    }
    return fromCodepoints(out);
}

std::vector<uint32_t> extractConsonants(const std::string& word) {
    auto cps = toCodepoints(word);
    std::vector<uint32_t> result;
    for (uint32_t cp : cps) {
        if (isArabicConsonant(cp) && !isDiacritic(cp)) {
            result.push_back(cp);
        }
    }
    return result;
}

int compare(const std::string& a, const std::string& b) {
    auto ca = toCodepoints(a);
    auto cb = toCodepoints(b);
    size_t len = std::min(ca.size(), cb.size());
    for (size_t i = 0; i < len; i++) {
        if (ca[i] < cb[i]) return -1;
        if (ca[i] > cb[i]) return  1;
    }
    if (ca.size() < cb.size()) return -1;
    if (ca.size() > cb.size()) return  1;
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Morphological pattern application
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Core generation algorithm.
 *
 * Scans the pattern template codepoint-by-codepoint.
 * - ف (FA)  → root[0]
 * - ع (AIN) → root[1]
 * - ل (LAM) → root[2]
 * - Any other codepoint → kept as-is
 *
 * This handles trilateral roots only (3 radicals).
 */
std::string applyPattern(const std::vector<uint32_t>& root,
                         const std::string& patternTemplate) {
    if (root.size() < 3) return "";

    auto pattern = toCodepoints(patternTemplate);
    std::vector<uint32_t> result;
    result.reserve(pattern.size());

    for (uint32_t cp : pattern) {
        if      (cp == FA)  result.push_back(root[0]);
        else if (cp == AIN) result.push_back(root[1]);
        else if (cp == LAM) result.push_back(root[2]);
        else                result.push_back(cp);
    }
    return fromCodepoints(result);
}

/**
 * Core validation algorithm.
 *
 * Given a word and a pattern template, attempts to extract the root consonants.
 * Steps:
 *   1. Strip diacritics from both word and pattern
 *   2. The pattern tells us: position i = ف/ع/ل → that position is a radical
 *   3. Collect the word's codepoints at those positions
 *   4. Return them in ف→r0, ع→r1, ل→r2 order
 *
 * Returns empty vector if the word length doesn't match the pattern.
 */
std::vector<uint32_t> extractRoot(const std::string& word,
                                   const std::string& patternTemplate) {
    // Strip diacritics for comparison
    auto wordCps    = toCodepoints(stripDiacritics(word));
    auto patternCps = toCodepoints(stripDiacritics(patternTemplate));

    if (wordCps.size() != patternCps.size()) return {};

    // Positions for each radical
    uint32_t r0 = 0, r1 = 0, r2 = 0;
    bool found0 = false, found1 = false, found2 = false;

    for (size_t i = 0; i < patternCps.size(); i++) {
        uint32_t pcp = patternCps[i];
        uint32_t wcp = wordCps[i];
        if      (pcp == FA)  { r0 = wcp; found0 = true; }
        else if (pcp == AIN) { r1 = wcp; found1 = true; }
        else if (pcp == LAM) { r2 = wcp; found2 = true; }
        else {
            // Fixed position – must match
            if (pcp != wcp) return {};
        }
    }

    if (!found0 || !found1 || !found2) return {};
    return { r0, r1, r2 };
}

bool matchesPattern(const std::string& word,
                    const std::vector<uint32_t>& root,
                    const std::string& patternTemplate) {
    if (root.size() < 3) return false;
    // Method 1: generate the expected word and compare
    std::string expected = applyPattern(root, patternTemplate);
    std::string wordStripped = stripDiacritics(word);
    std::string expectedStripped = stripDiacritics(expected);
    return wordStripped == expectedStripped;
}

int countRadicals(const std::string& patternTemplate) {
    auto cps = toCodepoints(patternTemplate);
    int count = 0;
    for (uint32_t cp : cps) {
        if (cp == FA || cp == AIN || cp == LAM) count++;
    }
    return count;
}

} // namespace ArabicUtils
