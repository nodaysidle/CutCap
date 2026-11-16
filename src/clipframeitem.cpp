#include "clipframeitem.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QUrl>
#include <QVideoFrameFormat>
#include <QtConcurrent/QtConcurrent>
#include <QtQml/qqml.h>
#include <algorithm>
#include <cmath>
#include <vector>

bool ClipLook::isIdentity() const {
    return std::abs(brightness) < 1e-3 && std::abs(contrast - 1.0) < 1e-3 && std::abs(saturation - 1.0) < 1e-3 &&
           blurRadius < 0.5 && !vignette && !chromaKey;
}

bool ClipLook::operator==(const ClipLook &o) const {
    return brightness == o.brightness && contrast == o.contrast && saturation == o.saturation &&
           blurRadius == o.blurRadius && vignette == o.vignette && chromaKey == o.chromaKey && keyColor == o.keyColor &&
           similarity == o.similarity && blend == o.blend && bt709 == o.bt709;
}

namespace {

struct Matrix {
    // Limited-range R'G'B' (0..255) <-> Y'CbCr codes, as stored in yuv420p video.
    double yr, yg, yb, ur, ug, ub, vr, vg, vb;
    double rv, gu, gv, bu;
};

constexpr Matrix kBt601{0.256788, 0.504129, 0.097906, -0.148223, -0.290993, 0.439216, 0.439216, -0.367788, -0.071427,
                        1.596027, -0.391762, -0.812968, 2.017232};
constexpr Matrix kBt709{0.182586, 0.614231, 0.062007, -0.100644, -0.338572, 0.439216, 0.439216, -0.398942, -0.040274,
                        1.792741, -0.213249, -0.532909, 2.112402};

// One horizontal + one vertical running-sum pass: boxblur with power 1, edges replicated.
void boxBlur(std::vector<float> &plane, int w, int h, int radius) {
    radius = std::min({radius, (w - 1) / 2, (h - 1) / 2});
    if (radius < 1)
        return;
    const float norm = 1.0f / float(2 * radius + 1);
    std::vector<float> line(std::max(w, h));
    for (int y = 0; y < h; ++y) {
        float *row = plane.data() + size_t(y) * w;
        std::copy(row, row + w, line.begin());
        float sum = line[0] * float(radius + 1);
        for (int i = 1; i <= radius; ++i)
            sum += line[std::min(i, w - 1)];
        for (int x = 0; x < w; ++x) {
            row[x] = sum * norm;
            sum += line[std::min(x + radius + 1, w - 1)] - line[std::max(x - radius, 0)];
        }
    }
    for (int x = 0; x < w; ++x) {
        for (int y = 0; y < h; ++y)
            line[y] = plane[size_t(y) * w + x];
        float sum = line[0] * float(radius + 1);
        for (int i = 1; i <= radius; ++i)
            sum += line[std::min(i, h - 1)];
        for (int y = 0; y < h; ++y) {
            plane[size_t(y) * w + x] = sum * norm;
            sum += line[std::min(y + radius + 1, h - 1)] - line[std::max(y - radius, 0)];
        }
    }
}

} // namespace

QImage ClipFrameItem::process(const QImage &source, const ClipLook &look) {
    if (source.isNull())
        return {};
    if (look.isIdentity())
        return source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const QImage in = source.convertToFormat(QImage::Format_ARGB32);
    const int w = in.width();
    const int h = in.height();
    const size_t n = size_t(w) * h;
    const Matrix &m = look.bt709 ? kBt709 : kBt601;
    std::vector<float> Y(n), U(n), V(n), A(n);

    for (int y = 0; y < h; ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(in.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const double r = qRed(px), g = qGreen(px), b = qBlue(px);
            const size_t i = size_t(y) * w + x;
            Y[i] = float(16.0 + m.yr * r + m.yg * g + m.yb * b);
            U[i] = float(128.0 + m.ur * r + m.ug * g + m.ub * b);
            V[i] = float(128.0 + m.vr * r + m.vg * g + m.vb * b);
            A[i] = float(qAlpha(px));
        }
    }

    // eq: brightness/contrast on luma codes, saturation scales chroma around 128.
    if (std::abs(look.brightness) >= 1e-3 || std::abs(look.contrast - 1.0) >= 1e-3 || std::abs(look.saturation - 1.0) >= 1e-3) {
        for (size_t i = 0; i < n; ++i) {
            const double v = look.contrast * (Y[i] / 255.0 - 0.5) + 0.5 + look.brightness;
            Y[i] = float(std::clamp(v * 256.0, 0.0, 255.0));
            U[i] = float(std::clamp((U[i] - 128.0) * look.saturation + 128.0, 0.0, 255.0));
            V[i] = float(std::clamp((V[i] - 128.0) * look.saturation + 128.0, 0.0, 255.0));
        }
    }

    // boxblur=r:1 — chroma planes are half resolution in 4:2:0, so their radius covers twice the pixels.
    if (look.blurRadius >= 0.5) {
        const int r = int(std::lround(look.blurRadius));
        boxBlur(Y, w, h, r);
        boxBlur(U, w, h, 2 * r);
        boxBlur(V, w, h, 2 * r);
    }

    if (look.vignette) {
        const double dmax = std::hypot(w / 2.0, h / 2.0);
        const double angle = M_PI / 5.0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const double dnorm = std::hypot(x - w / 2.0, y - h / 2.0) / dmax;
                const double c = std::cos(angle * dnorm);
                const double f = dnorm > 1.0 ? 0.0 : (c * c) * (c * c);
                const size_t i = size_t(y) * w + x;
                Y[i] = float(Y[i] * f);
                U[i] = float((U[i] - 127.0) * f + 127.0);
                V[i] = float((V[i] - 127.0) * f + 127.0);
            }
        }
    }

    if (look.chromaKey) {
        // The key colour is converted with full-range coefficients, exactly as vf_chromakey does.
        const double kr = look.keyColor.red(), kg = look.keyColor.green(), kb = look.keyColor.blue();
        const double keyU = -0.16874 * kr - 0.33126 * kg + 0.5 * kb + 128.0;
        const double keyV = 0.5 * kr - 0.41869 * kg - 0.08131 * kb + 128.0;
        for (size_t i = 0; i < n; ++i) {
            const double du = U[i] - keyU;
            const double dv = V[i] - keyV;
            const double diff = std::sqrt((du * du + dv * dv) / (255.0 * 255.0 * 2.0));
            const double keep = look.blend > 0.0001 ? std::clamp((diff - look.similarity) / look.blend, 0.0, 1.0)
                                                    : (diff > look.similarity ? 1.0 : 0.0);
            A[i] = float(A[i] * keep);
        }
    }

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const size_t i = size_t(y) * w + x;
            const double yy = 1.164384 * (Y[i] - 16.0);
            const double u = U[i] - 128.0;
            const double v = V[i] - 128.0;
            const double a = std::clamp(double(A[i]), 0.0, 255.0) / 255.0;
            const int r = int(std::lround(std::clamp(yy + m.rv * v, 0.0, 255.0) * a));
            const int g = int(std::lround(std::clamp(yy + m.gu * u + m.gv * v, 0.0, 255.0) * a));
            const int b = int(std::lround(std::clamp(yy + m.bu * u, 0.0, 255.0) * a));
            line[x] = qRgba(r, g, b, int(std::lround(a * 255.0)));
        }
    }
    return out;
}

ClipFrameItem::ClipFrameItem(QQuickItem *parent) : QQuickItem(parent), m_sink(new QVideoSink(this)) {
    setFlag(ItemHasContents, true);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        // Frames queue up from the decoder thread; only the newest one is worth processing.
        if (!m_imageSource.isEmpty() || !frame.isValid() || frame != m_sink->videoFrame())
            return;
        setSource(frame.toImage(), frame.surfaceFormat().colorSpace() == QVideoFrameFormat::ColorSpace_BT709);
    });
    connect(&m_watcher, &QFutureWatcher<QImage>::finished, this, [this] {
        const bool hadFrame = hasFrame();
        m_output = m_watcher.result();
        m_textureDirty = true;
        update();
        if (hadFrame != hasFrame())
            emit hasFrameChanged();
        if (m_dirtyWhileBusy) {
            m_dirtyWhileBusy = false;
            scheduleProcessing();
        }
    });
    connect(this, &ClipFrameItem::lookChanged, this, &ClipFrameItem::scheduleProcessing);
}

ClipFrameItem::~ClipFrameItem() {
    m_watcher.waitForFinished();
}

void ClipFrameItem::registerQmlType() {
    qmlRegisterType<ClipFrameItem>("CutCap", 1, 0, "ClipFrameItem");
}

void ClipFrameItem::setImageSource(const QString &source) {
    if (source == m_imageSource)
        return;
    m_imageSource = source;
    emit imageSourceChanged();
    const QString path = source.startsWith("file:") ? QUrl(source).toLocalFile() : source;
    setSource(source.isEmpty() ? QImage() : QImage(path), false);
}

#define CUTCAP_SETTER(Name, Type, member)  \
    void ClipFrameItem::Name(Type v) {     \
        if (member == v)                   \
            return;                        \
        member = v;                        \
        emit lookChanged();                \
    }
CUTCAP_SETTER(setBrightness, double, m_brightness)
CUTCAP_SETTER(setContrast, double, m_contrast)
CUTCAP_SETTER(setSaturation, double, m_saturation)
CUTCAP_SETTER(setBlur, double, m_blur)
CUTCAP_SETTER(setReferenceHeight, double, m_referenceHeight)
CUTCAP_SETTER(setVignette, bool, m_vignette)
CUTCAP_SETTER(setChromaKey, bool, m_chromaKey)
CUTCAP_SETTER(setKeyColor, const QColor &, m_keyColor)
CUTCAP_SETTER(setSimilarity, double, m_similarity)
CUTCAP_SETTER(setBlend, double, m_blend)
#undef CUTCAP_SETTER

void ClipFrameItem::setSource(const QImage &image, bool bt709) {
    m_source = image;
    m_sourceBt709 = bt709;
    scheduleProcessing();
}

ClipLook ClipFrameItem::currentLook() const {
    ClipLook look;
    look.brightness = m_brightness;
    look.contrast = m_contrast;
    look.saturation = m_saturation;
    look.vignette = m_vignette;
    look.chromaKey = m_chromaKey;
    look.keyColor = m_keyColor;
    look.similarity = m_similarity;
    look.blend = m_blend;
    look.bt709 = m_sourceBt709;
    return look;
}

void ClipFrameItem::scheduleProcessing() {
    if (m_watcher.isRunning()) {
        m_dirtyWhileBusy = true;
        return;
    }
    if (m_source.isNull()) {
        if (hasFrame()) {
            m_output = {};
            m_textureDirty = true;
            update();
            emit hasFrameChanged();
        }
        return;
    }
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const QSize target = (width() > 0 && height() > 0) ? QSize(int(std::ceil(width() * dpr)), int(std::ceil(height() * dpr)))
                                                      : QSize(1280, 1280);
    const QImage source = m_source;
    ClipLook look = currentLook();
    const double blur = m_blur;
    const double referenceHeight = m_referenceHeight;
    m_watcher.setFuture(QtConcurrent::run([source, target, look, blur, referenceHeight]() mutable {
        const QImage scaled = (source.width() > target.width() || source.height() > target.height())
                                  ? source.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                  : source;
        // Blur is given in export pixels for a clip `referenceHeight` tall; scale it to this image.
        look.blurRadius = referenceHeight > 0.0 ? blur * scaled.height() / referenceHeight : blur;
        return process(scaled, look);
    }));
}

QSGNode *ClipFrameItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (m_output.isNull() || width() <= 0 || height() <= 0) {
        delete node;
        return nullptr;
    }
    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
        m_textureDirty = true;
    }
    if (m_textureDirty) {
        // ownsTexture: setTexture() deletes the previous texture itself.
        node->setTexture(window()->createTextureFromImage(m_output, QQuickWindow::TextureHasAlphaChannel));
        m_textureDirty = false;
    }
    const QSizeF img = m_output.size();
    const double s = std::min(width() / img.width(), height() / img.height());
    const QSizeF fitted(img.width() * s, img.height() * s);
    node->setRect(QRectF(QPointF((width() - fitted.width()) / 2.0, (height() - fitted.height()) / 2.0), fitted));
    return node;
}
