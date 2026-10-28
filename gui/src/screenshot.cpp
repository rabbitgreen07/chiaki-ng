#include "screenshot.h"

#include <QDir>
#include <QStandardPaths>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

struct Coeffs { double kr, kb; };

Coeffs MatrixFor(const AVFrame *f)
{
    switch (f->colorspace) {
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        return {0.2627, 0.0593};
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        return {0.299, 0.114};
    case AVCOL_SPC_BT709:
        return {0.2126, 0.0722};
    default:
        // Unspecified: HD content is BT.709 in practice.
        return f->height >= 720 ? Coeffs{0.2126, 0.0722} : Coeffs{0.299, 0.114};
    }
}

inline uint32_t ReadComponent(const AVFrame *f, const AVComponentDescriptor &c, int x, int y)
{
    const uint8_t *p = f->data[c.plane] + (ptrdiff_t)y * f->linesize[c.plane] + (ptrdiff_t)x * c.step + c.offset;
    uint32_t v = c.depth > 8 ? (uint32_t)(p[0] | (p[1] << 8)) : p[0];
    return (v >> c.shift) & ((1u << c.depth) - 1);
}

// SMPTE ST 2084 (PQ) EOTF: non-linear [0,1] -> linear nits
inline double PqToNits(double e)
{
    const double m1 = 2610.0 / 16384.0, m2 = 2523.0 / 4096.0 * 128.0;
    const double c1 = 3424.0 / 4096.0, c2 = 2413.0 / 4096.0 * 32.0, c3 = 2392.0 / 4096.0 * 32.0;
    double ep = std::pow(std::max(e, 0.0), 1.0 / m2);
    double num = std::max(ep - c1, 0.0);
    return 10000.0 * std::pow(num / (c2 - c3 * ep), 1.0 / m1);
}

// ARIB STD-B67 (HLG) inverse OETF, scaled to a 1000 nit display (OOTF ignored)
inline double HlgToNits(double e)
{
    const double a = 0.17883277, b = 0.28466892, c = 0.55991073;
    double l = e <= 0.5 ? (e * e) / 3.0 : (std::exp((e - c) / a) + b) / 12.0;
    return 1000.0 * l;
}

inline double SrgbEncode(double l)
{
    l = std::clamp(l, 0.0, 1.0);
    return l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1.0 / 2.4) - 0.055;
}

} // namespace

QString ChiakiScreenshotDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QDir(base).filePath(QStringLiteral("chiaki-ng"));
}

QImage ChiakiFrameToImage(const AVFrame *src, QString *error)
{
    auto fail = [error](const QString &msg) {
        if (error)
            *error = msg;
        return QImage();
    };
    if (!src || src->width <= 0 || src->height <= 0)
        return fail(QStringLiteral("No video frame available"));

    AVFrame *sw = nullptr;
    const AVFrame *f = src;
    if (src->hw_frames_ctx) {
        sw = av_frame_alloc();
        if (!sw)
            return fail(QStringLiteral("Out of memory"));
        int r = av_hwframe_transfer_data(sw, src, 0);
        if (r < 0) {
            av_frame_free(&sw);
            return fail(QStringLiteral("Could not download frame from GPU (error %1)").arg(r));
        }
        // transfer_data does not carry colour metadata over
        sw->colorspace = src->colorspace;
        sw->color_range = src->color_range;
        sw->color_trc = src->color_trc;
        sw->color_primaries = src->color_primaries;
        f = sw;
    }

    const AVPixFmtDescriptor *desc = av_pix_fmt_desc_get((AVPixelFormat)f->format);
    if (!desc || (desc->flags & (AV_PIX_FMT_FLAG_RGB | AV_PIX_FMT_FLAG_BE | AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_PAL))
            || desc->nb_components < 3) {
        QString name = desc ? QString::fromLatin1(desc->name) : QStringLiteral("unknown");
        if (sw)
            av_frame_free(&sw);
        return fail(QStringLiteral("Unsupported pixel format %1").arg(name));
    }

    const AVComponentDescriptor &cy = desc->comp[0];
    const AVComponentDescriptor &cu = desc->comp[1];
    const AVComponentDescriptor &cv = desc->comp[2];
    const int depth = cy.depth;
    const double scale = double(1 << (depth - 8));
    const bool full_range = f->color_range == AVCOL_RANGE_JPEG;
    const double y_off = full_range ? 0.0 : 16.0 * scale;
    const double y_rng = full_range ? (255.0 * scale) : 219.0 * scale;
    const double c_off = 128.0 * scale;
    const double c_rng = full_range ? (255.0 * scale) : 224.0 * scale;

    const Coeffs k = MatrixFor(f);
    const double kg = 1.0 - k.kr - k.kb;
    const bool pq = f->color_trc == AVCOL_TRC_SMPTE2084;
    const bool hlg = f->color_trc == AVCOL_TRC_ARIB_STD_B67;
    const bool hdr = pq || hlg;
    const bool bt2020 = f->color_primaries == AVCOL_PRI_BT2020;

    // HDR tone mapping: SDR reference white is 203 nits (ITU-R BT.2408). Values
    // stay linear up to the knee, above it highlights roll off smoothly towards 1.
    const double ref_white = 203.0;
    const double knee = 0.75;

    QImage img(f->width, f->height, QImage::Format_RGB888);
    if (img.isNull()) {
        if (sw)
            av_frame_free(&sw);
        return fail(QStringLiteral("Out of memory"));
    }

    for (int y = 0; y < f->height; ++y) {
        uint8_t *out = img.scanLine(y);
        const int cy_row = y >> desc->log2_chroma_h;
        for (int x = 0; x < f->width; ++x) {
            const int cx = x >> desc->log2_chroma_w;
            double Y = (double(ReadComponent(f, cy, x, y)) - y_off) / y_rng;
            double U = (double(ReadComponent(f, cu, cx, cy_row)) - c_off) / c_rng;
            double V = (double(ReadComponent(f, cv, cx, cy_row)) - c_off) / c_rng;

            double r = Y + 2.0 * (1.0 - k.kr) * V;
            double b = Y + 2.0 * (1.0 - k.kb) * U;
            double g = (Y - k.kr * r - k.kb * b) / kg;

            if (hdr) {
                auto lin = [pq, ref_white](double e) { return (pq ? PqToNits(e) : HlgToNits(e)) / ref_white; };
                double lr = lin(std::clamp(r, 0.0, 1.0));
                double lg = lin(std::clamp(g, 0.0, 1.0));
                double lb = lin(std::clamp(b, 0.0, 1.0));
                if (bt2020) {
                    // BT.2020 -> BT.709 primaries (linear light)
                    double r7 = 1.6605 * lr - 0.5876 * lg - 0.0728 * lb;
                    double g7 = -0.1246 * lr + 1.1329 * lg - 0.0083 * lb;
                    double b7 = -0.0182 * lr - 0.1006 * lg + 1.1187 * lb;
                    lr = std::max(r7, 0.0);
                    lg = std::max(g7, 0.0);
                    lb = std::max(b7, 0.0);
                }
                // Scale by the brightest channel so hue is preserved and nothing clips.
                double m = std::max({lr, lg, lb});
                if (m > knee) {
                    double mm = knee + (1.0 - knee) * (1.0 - std::exp(-(m - knee) / (1.0 - knee)));
                    double s = mm / m;
                    lr *= s; lg *= s; lb *= s;
                }
                r = SrgbEncode(lr);
                g = SrgbEncode(lg);
                b = SrgbEncode(lb);
            }

            out[3 * x + 0] = (uint8_t)std::lround(std::clamp(r, 0.0, 1.0) * 255.0);
            out[3 * x + 1] = (uint8_t)std::lround(std::clamp(g, 0.0, 1.0) * 255.0);
            out[3 * x + 2] = (uint8_t)std::lround(std::clamp(b, 0.0, 1.0) * 255.0);
        }
    }

    if (sw)
        av_frame_free(&sw);
    return img;
}
