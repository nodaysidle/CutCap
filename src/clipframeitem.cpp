#include "clipframeitem.h"

#include <QQuickWindow>
#include <QSGGeometry>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGNode>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <QUrl>
#include <QVideoFrameFormat>
#include <QtConcurrent/QtConcurrent>
#include <QtQml/qqml.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
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

namespace {

std::atomic<qint64> s_cpuPixelsProcessed{0};

} // namespace

qint64 ClipFrameItem::cpuPixelsProcessed() {
    return s_cpuPixelsProcessed.load();
}

QImage ClipFrameItem::process(const QImage &source, const ClipLook &look) {
    if (source.isNull())
        return {};
    if (look.isIdentity())
        return source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const QImage in = source.convertToFormat(QImage::Format_ARGB32);
    const int w = in.width();
    const int h = in.height();
    const size_t n = size_t(w) * h;
    s_cpuPixelsProcessed += qint64(n);
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

namespace {

// The look shader's blur loop has a fixed tap budget (MAXR in look.frag); a frame whose blur
// radius in image pixels exceeds this is still processed exactly, on the CPU.
constexpr double kMaxShaderBlurRadius = 16.0;
constexpr int kLookUniformVecs = 9; // keep in step with look.frag's u[9]

class LookShader : public QSGMaterialShader {
public:
    LookShader();
    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) override;
    void updateSampledImage(RenderState &state, int binding, QSGTexture **texture, QSGMaterial *newMaterial,
                            QSGMaterial *oldMaterial) override;
};

// Inherits QSGTextureMaterial so the texture binding path is exactly Qt's own; only the shader
// (and the extra look uniforms) are ours.
class LookMaterial : public QSGTextureMaterial {
public:
    LookMaterial() {
        setFlag(Blending);   // the shader outputs premultiplied alpha (chroma key)
        setFlag(NoBatching); // one quad per item; per-node uniforms must not be merged
    }
    QSGMaterialType *type() const override { return &s_type; }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override { return new LookShader; }
    int compare(const QSGMaterial *other) const override {
        if (other->type() != type())
            return 1;
        const auto *o = static_cast<const LookMaterial *>(other);
        return (texture() == o->texture() && std::memcmp(m_uniforms, o->m_uniforms, sizeof(m_uniforms)) == 0) ? 0 : 1;
    }

    float m_uniforms[kLookUniformVecs * 4] = {};
    static QSGMaterialType s_type;
};

QSGMaterialType LookMaterial::s_type;

LookShader::LookShader() {
    // The .qsb packs next to the .vert/.frag sources are baked with qsb (see look.frag).
    setShaderFileName(VertexStage, QStringLiteral(":/shaders/look.vert.qsb"));
    setShaderFileName(FragmentStage, QStringLiteral(":/shaders/look.frag.qsb"));
}

bool LookShader::updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *oldMaterial) {
    const auto *m = static_cast<const LookMaterial *>(newMaterial);
    bool dirty = false;
    QByteArray *buf = state.uniformData();
    if (state.isMatrixDirty()) {
        const QMatrix4x4 matrix = state.combinedMatrix();
        std::memcpy(buf->data(), matrix.constData(), 64);
        dirty = true;
    }
    if (state.isOpacityDirty()) {
        const float opacity = state.opacity();
        std::memcpy(buf->data() + 64, &opacity, 4);
        dirty = true;
    }
    const auto *old = oldMaterial ? static_cast<const LookMaterial *>(oldMaterial) : nullptr;
    if (!old || old->texture() != m->texture() ||
        std::memcmp(old->m_uniforms, m->m_uniforms, sizeof(m->m_uniforms)) != 0) {
        // std140: mat4 at 0, float at 64, the vec4 array u[9] starts at the next 16-byte boundary.
        Q_ASSERT(buf->size() >= 80 + int(sizeof(m->m_uniforms)));
        std::memcpy(buf->data() + 80, m->m_uniforms, sizeof(m->m_uniforms));
        dirty = true;
    }
    return dirty;
}

void LookShader::updateSampledImage(RenderState &state, int binding, QSGTexture **texture, QSGMaterial *newMaterial, QSGMaterial *) {
    if (binding != 1)
        return;
    QSGTexture *t = static_cast<LookMaterial *>(newMaterial)->texture();
    *texture = t;
    // Uploads the image data to the QRhiTexture if not done yet; without this the sampler
    // would bind the renderer's dummy texture.
    if (t)
        t->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
}

// A textured quad drawn through LookMaterial: the frame texture in, the look applied per pixel
// in the fragment shader. QSGSimpleTextureNode owns the geometry and the texture; the material
// is swapped for ours so the look runs in the shader instead of on the CPU.
class LookNode : public QSGSimpleTextureNode {
public:
    LookNode() {
        setOwnsTexture(true);
        setFiltering(QSGTexture::Linear);
        // QSGSimpleTextureNode wires its own texture materials in its constructor; replace both
        // so the renderer picks up ours.
        setMaterial(new LookMaterial);
        setOpaqueMaterial(nullptr);
        setFlag(OwnsMaterial);
    }

    void setTexture(QSGTexture *texture) {
        QSGSimpleTextureNode::setTexture(texture);
        texture->setFiltering(QSGTexture::Linear);
        texture->setMipmapFiltering(QSGTexture::None); // no mipmaps: a mipmap sampler would be incomplete
        texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
        static_cast<LookMaterial *>(material())->setTexture(texture);
    }

    // Fills the shader uniforms from the look, with the same clamping process() applies.
    void setLook(const ClipLook &look, const QSize &texSize) {
        const Matrix &m = look.bt709 ? kBt709 : kBt601;
        const int w = texSize.width();
        const int h = texSize.height();
        const bool eqActive = std::abs(look.brightness) >= 1e-3 || std::abs(look.contrast - 1.0) >= 1e-3 ||
                              std::abs(look.saturation - 1.0) >= 1e-3;
        const int rRaw = look.blurRadius >= 0.5 ? int(std::lround(look.blurRadius)) : 0;
        const int rMax = std::min((w - 1) / 2, (h - 1) / 2);
        const int rY = std::min(rRaw, rMax);
        const int rC = std::min(2 * rRaw, rMax);
        const double kr = look.keyColor.red(), kg = look.keyColor.green(), kb = look.keyColor.blue();
        const double keyU = -0.16874 * kr - 0.33126 * kg + 0.5 * kb + 128.0;
        const double keyV = 0.5 * kr - 0.41869 * kg - 0.08131 * kb + 128.0;
        const double dmax = std::hypot(w / 2.0, h / 2.0);

        float *u = static_cast<LookMaterial *>(material())->m_uniforms;
        const auto put = [&](int i, float x, float y, float z, float v) {
            u[i * 4 + 0] = x;
            u[i * 4 + 1] = y;
            u[i * 4 + 2] = z;
            u[i * 4 + 3] = v;
        };
        put(0, float(m.yr), float(m.yg), float(m.yb), 16.0f);
        put(1, float(m.ur), float(m.ug), float(m.ub), 128.0f);
        put(2, float(m.vr), float(m.vg), float(m.vb), 128.0f);
        put(3, float(m.rv), float(m.gu), float(m.gv), float(m.bu));
        put(4, eqActive ? 1.0f : 0.0f, float(look.brightness), float(look.contrast), float(look.saturation));
        put(5, float(rY), float(rC), look.vignette ? 1.0f : 0.0f, look.chromaKey ? 1.0f : 0.0f);
        put(6, float(keyU), float(keyV), float(look.similarity), float(look.blend));
        put(7, float(w), float(h), float(dmax), 1.164384f);
        put(8, 1.0f / float(w), 1.0f / float(h), 0.0f, 0.0f);
        markDirty(QSGNode::DirtyMaterial);
    }
};

} // namespace

ClipFrameItem::ClipFrameItem(QQuickItem *parent) : QQuickItem(parent), m_sink(new QVideoSink(this)) {
    setFlag(ItemHasContents, true);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
        // Frames queue up from the decoder thread; only the newest one is worth processing.
        if (!m_imageSource.isEmpty() || !frame.isValid() || frame != m_sink->videoFrame())
            return;
        setVideoFrame(frame);
    });
    connect(&m_watcher, &QFutureWatcher<QImage>::finished, this, [this] {
        const bool hadFrame = hasFrame();
        m_output = m_watcher.result();
        m_outputProcessed = m_lastRunCpu.load();
        m_textureDirty = true;
        update();
        if (hadFrame != hasFrame())
            emit hasFrameChanged();
        if (m_dirtyWhileBusy) {
            m_dirtyWhileBusy = false;
            scheduleProcessing();
        }
    });
    connect(this, &ClipFrameItem::lookChanged, this, &ClipFrameItem::applyLookChange);
    connect(this, &QQuickItem::visibleChanged, this, &ClipFrameItem::resumeIfPending);
}

ClipFrameItem::~ClipFrameItem() {
    m_watcher.waitForFinished();
}

void ClipFrameItem::setImageSource(const QString &source) {
    if (source == m_imageSource)
        return;
    m_imageSource = source;
    emit imageSourceChanged();
    if (source.isEmpty()) {
        setSource(QImage(), false);
        return;
    }
    // Decoding a large photo takes long enough to stall the UI; do it in a worker and take the
    // result only if this is still the image being shown.
    const QString path = source.startsWith("file:") ? QUrl(source).toLocalFile() : source;
    auto *loader = new QFutureWatcher<QImage>(this);
    connect(loader, &QFutureWatcher<QImage>::finished, this, [this, loader, source] {
        loader->deleteLater();
        if (source == m_imageSource)
            setSource(loader->result(), false);
    });
    loader->setFuture(QtConcurrent::run([path] { return QImage(path); }));
}

// Hidden, or in a window the compositor is not showing (another workspace, minimised): Qt reports
// such a window as not exposed and renders nothing, so processing frames for it is wasted work.
bool ClipFrameItem::canBeSeen() const {
    return isVisible() && (!window() || window()->isExposed());
}

void ClipFrameItem::resumeIfPending() {
    if (m_pendingWhileUnseen && canBeSeen()) {
        m_pendingWhileUnseen = false;
        scheduleProcessing();
    }
}

void ClipFrameItem::itemChange(ItemChange change, const ItemChangeData &data) {
    // A swapped frame is the sign the window is being shown again after it was not exposed.
    if (change == ItemSceneChange && data.window)
        connect(data.window, &QQuickWindow::frameSwapped, this, &ClipFrameItem::resumeIfPending,
                Qt::UniqueConnection);
    QQuickItem::itemChange(change, data);
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
    m_sourceFrame = QVideoFrame();
    m_source = image;
    m_sourceBt709 = bt709;
    scheduleProcessing();
}

void ClipFrameItem::setVideoFrame(const QVideoFrame &frame) {
    m_sourceFrame = frame;
    m_source = QImage();
    m_sourceBt709 = frame.surfaceFormat().colorSpace() == QVideoFrameFormat::ColorSpace_BT709;
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

double ClipFrameItem::effectiveBlurRadius() const {
    if (m_output.isNull())
        return 0.0;
    // Blur is given in export pixels for a clip `referenceHeight` tall; scale it to this image.
    return m_referenceHeight > 0.0 ? m_blur * m_output.height() / m_referenceHeight : m_blur;
}

// A look change on a frame that is already on the GPU only moves uniforms: the scene graph
// re-runs the shader, no pixel is touched on the CPU.
void ClipFrameItem::applyLookChange() {
    if (!canBeSeen()) {
        m_pendingWhileUnseen = true;
        return;
    }
    if (m_watcher.isRunning()) {
        m_dirtyWhileBusy = true;
        return;
    }
    if (!hasFrame() || m_outputProcessed || effectiveBlurRadius() > kMaxShaderBlurRadius) {
        scheduleProcessing();
        return;
    }
    update();
}

void ClipFrameItem::scheduleProcessing() {
    if (!canBeSeen()) {
        m_pendingWhileUnseen = true;
        return;
    }
    if (m_watcher.isRunning()) {
        m_dirtyWhileBusy = true;
        return;
    }
    if (m_source.isNull() && !m_sourceFrame.isValid()) {
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
    const QVideoFrame frame = m_sourceFrame;
    ClipLook look = currentLook();
    const double blur = m_blur;
    const double referenceHeight = m_referenceHeight;
    m_watcher.setFuture(QtConcurrent::run([source, frame, target, look, blur, referenceHeight, this]() mutable {
        // Downloading and converting the frame is the expensive part; it belongs on this thread.
        // The look itself runs in the shader at paint time, not here.
        const QImage decoded = source.isNull() && frame.isValid() ? frame.toImage() : source;
        if (decoded.isNull())
            return QImage();
        const QImage scaled = (decoded.width() > target.width() || decoded.height() > target.height())
                                  ? decoded.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                                  : decoded;
        // Blur is given in export pixels for a clip `referenceHeight` tall; scale it to this image.
        look.blurRadius = referenceHeight > 0.0 ? blur * scaled.height() / referenceHeight : blur;
        if (!look.isIdentity() && look.blurRadius > kMaxShaderBlurRadius) {
            // Beyond the shader's tap budget: keep the frame exact on the CPU.
            m_lastRunCpu = true;
            return process(scaled, look);
        }
        m_lastRunCpu = false;
        return scaled.convertToFormat(QImage::Format_ARGB32);
    }));
}

QSGNode *ClipFrameItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    if (m_output.isNull() || width() <= 0 || height() <= 0) {
        delete oldNode;
        return nullptr;
    }
    ClipLook look = currentLook();
    look.blurRadius = effectiveBlurRadius();
    const QSizeF img = m_output.size();
    const double s = std::min(width() / img.width(), height() / img.height());
    const QSizeF fitted(img.width() * s, img.height() * s);
    const QRectF rect(QPointF((width() - fitted.width()) / 2.0, (height() - fitted.height()) / 2.0), fitted);

    // Frames that came back from the CPU fallback, or that need no look at all, are plain
    // textures; every other look is applied by the shader while painting.
    if (m_outputProcessed || look.isIdentity()) {
        // A LookNode must not be reused here: its material would keep applying the old look.
        auto *node = dynamic_cast<LookNode *>(oldNode) ? nullptr : dynamic_cast<QSGSimpleTextureNode *>(oldNode);
        if (!node) {
            delete oldNode;
            node = new QSGSimpleTextureNode;
            node->setOwnsTexture(true);
            node->setFiltering(QSGTexture::Linear);
            m_textureDirty = true;
        }
        if (m_textureDirty) {
            // ownsTexture: setTexture() deletes the previous texture itself.
            const QImage upload = m_output.format() == QImage::Format_ARGB32_Premultiplied
                                     ? m_output
                                     : m_output.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            node->setTexture(window()->createTextureFromImage(upload, QQuickWindow::TextureHasAlphaChannel));
            m_textureDirty = false;
        }
        node->setRect(rect);
        return node;
    }

    auto *node = dynamic_cast<LookNode *>(oldNode);
    if (!node) {
        delete oldNode;
        node = new LookNode;
        m_textureDirty = true;
    }
    if (m_textureDirty) {
        node->setTexture(window()->createTextureFromImage(m_output));
        m_textureDirty = false;
    }
    node->setRect(rect);
    node->setLook(look, m_output.size());
    return node;
}
