// SPDX-License-Identifier: MIT
#include "core/numbers.h"

#include <charconv>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {

double roundHalfUp(double v)
{
    // std::round() rounds halves away from zero, which differs for negatives
    double r = std::floor(v);
    if (v - r >= 0.5)
        r += 1;
    return r;
}

double roundTo(double v, int decimals)
{
    const double scale = std::pow(10.0, decimals);
    return roundHalfUp(v * scale) / scale;
}

QString formatNumber(double v)
{
    if (std::isnan(v))
        return u"NaN"_s;
    if (std::isinf(v))
        return v > 0 ? u"Infinity"_s : u"-Infinity"_s;
    if (v == 0)
        return u"0"_s; // also catches -0

    const double magnitude = std::abs(v);
    const bool plain = magnitude >= 1e-6 && magnitude < 1e21;
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof buf, v,
                                   plain ? std::chars_format::fixed : std::chars_format::scientific);
    QString out = QString::fromLatin1(buf, res.ptr - buf);

    // "1e-07" -> "1e-7"
    if (const qsizetype e = out.indexOf(u'e'); e >= 0) {
        qsizetype digits = e + 2;
        while (digits < out.size() - 1 && out.at(digits) == u'0')
            out.remove(digits, 1);
    }
    return out;
}

} // namespace pou
