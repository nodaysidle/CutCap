#pragma once

#include <QColor>
#include <QFutureWatcher>
#include <QImage>
#include <QQuickItem>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

// Clip effects applied with the same math as the FFmpeg filters used on export
// (eq -> boxblur -> vignette -> chromakey, in that order, on Y'CbCr values),
// so what the preview shows is what the exported file contains.
struct ClipLook {
    double brightness = 0.0; // eq brightness
    double contrast = 1.0;   // eq contrast
    double saturation = 1.0; // eq saturation
    double blurRadius = 0.0; // boxblur luma/chroma radius, in pixels of the processed image
    bool vignette = false;   // vignette with FFmpeg's default angle (PI/5)
    bool chromaKey = false;
    QColor keyColor = QColor(0, 255, 0);
    double similarity = 0.18;
    double blend = 0.08;
    bool bt709 = false; // matrix used to turn the decoded RGB back into the codes FFmpeg filters

    bool isIdentity() const;
    bool operator==(const ClipLook &o) const;
};

class ClipFrameItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVideoSink *videoSink READ videoSink CONSTANT)
    Q_PROPERTY(QString imageSource READ imageSource WRITE setImageSource NOTIFY imageSourceChanged)
    Q_PROPERTY(double brightness MEMBER m_brightness WRITE setBrightness NOTIFY lookChanged)
    Q_PROPERTY(double contrast MEMBER m_contrast WRITE setContrast NOTIFY lookChanged)
    Q_PROPERTY(double saturation MEMBER m_saturation WRITE setSaturation NOTIFY lookChanged)
    Q_PROPERTY(double blur MEMBER m_blur WRITE setBlur NOTIFY lookChanged)
    Q_PROPERTY(double referenceHeight MEMBER m_referenceHeight WRITE setReferenceHeight NOTIFY lookChanged)
    Q_PROPERTY(bool vignette MEMBER m_vignette WRITE setVignette NOTIFY lookChanged)
    Q_PROPERTY(bool chromaKey MEMBER m_chromaKey WRITE setChromaKey NOTIFY lookChanged)
    Q_PROPERTY(QColor keyColor MEMBER m_keyColor WRITE setKeyColor NOTIFY lookChanged)
    Q_PROPERTY(double similarity MEMBER m_similarity WRITE setSimilarity NOTIFY lookChanged)
    Q_PROPERTY(double blend MEMBER m_blend WRITE setBlend NOTIFY lookChanged)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged)

public:
    explicit ClipFrameItem(QQuickItem *parent = nullptr);
    ~ClipFrameItem() override;

    QVideoSink *videoSink() const { return m_sink; }
    QString imageSource() const { return m_imageSource; }
    void setImageSource(const QString &source);
    bool hasFrame() const { return !m_output.isNull(); }

    void setBrightness(double v);
    void setContrast(double v);
    void setSaturation(double v);
    void setBlur(double v);
    void setReferenceHeight(double v);
    void setVignette(bool v);
    void setChromaKey(bool v);
    void setKeyColor(const QColor &v);
    void setSimilarity(double v);
    void setBlend(double v);

    static QImage process(const QImage &source, const ClipLook &look);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

signals:
    void imageSourceChanged();
    void lookChanged();
    void hasFrameChanged();

private:
    void setSource(const QImage &image, bool bt709);
    void setVideoFrame(const QVideoFrame &frame);
    void scheduleProcessing();
    ClipLook currentLook() const;

    QVideoSink *m_sink;
    QString m_imageSource;
    QImage m_source;
    QVideoFrame m_sourceFrame; // decoded frame awaiting conversion, converted in the worker
    bool m_sourceBt709 = false;
    QImage m_output;
    bool m_textureDirty = false;
    bool m_dirtyWhileBusy = false;
    QFutureWatcher<QImage> m_watcher;

    double m_brightness = 0.0;
    double m_contrast = 1.0;
    double m_saturation = 1.0;
    double m_blur = 0.0;
    double m_referenceHeight = 0.0;
    bool m_vignette = false;
    bool m_chromaKey = false;
    QColor m_keyColor = QColor(0, 255, 0);
    double m_similarity = 0.18;
    double m_blend = 0.08;
};
