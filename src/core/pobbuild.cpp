// SPDX-License-Identifier: MIT
#include "core/pobbuild.h"

#include <QByteArray>
#include <zlib.h>

namespace pou {
namespace {

// A build is a few hundred kB of XML, so whatever inflates past this isn't one.
constexpr qsizetype maxXmlSize = 32 * 1024 * 1024;

// windowBits as zlib takes them: 15 + 32 reads zlib or gzip data, -15 raw deflate.
std::optional<QByteArray> inflated(const QByteArray &data, int windowBits)
{
    z_stream stream{};
    if (data.size() > maxXmlSize || inflateInit2(&stream, windowBits) != Z_OK)
        return std::nullopt;

    QByteArray out;
    char chunk[16384];
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = uInt(data.size());
    int status = Z_OK;
    while (status == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef *>(chunk);
        stream.avail_out = sizeof chunk;
        status = inflate(&stream, Z_NO_FLUSH);
        out.append(chunk, qsizetype(sizeof chunk - stream.avail_out));
        if (out.size() > maxXmlSize)
            status = Z_MEM_ERROR;
    }
    inflateEnd(&stream);
    if (status != Z_STREAM_END)
        return std::nullopt;
    return out;
}

} // namespace

std::optional<QString> decodeBuildCode(const QString &code)
{
    const QString trimmed = code.trimmed();
    if (trimmed.startsWith(u'<'))
        return trimmed;

    // Codes use the URL-safe alphabet, but arrive wrapped over lines or
    // converted to the standard one.
    QByteArray base64 = trimmed.toLatin1();
    base64.removeIf([](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; });
    base64.replace('+', '-').replace('/', '_');
    const auto decoded = QByteArray::fromBase64Encoding(base64, QByteArray::Base64UrlEncoding |
                                                                    QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded || decoded.decoded.isEmpty())
        return std::nullopt;

    std::optional<QByteArray> xml = inflated(decoded.decoded, 15 + 32);
    if (!xml)
        xml = inflated(decoded.decoded, -15);
    if (!xml)
        return std::nullopt;
    return QString::fromUtf8(*xml);
}

} // namespace pou
