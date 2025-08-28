#ifndef RKNNFACEDETECTOR_H
#define RKNNFACEDETECTOR_H

#include <QObject>
#include <QImage>
#include <QList>
#include <QRect>
#include "myconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <rknn_api.h>
#ifdef __cplusplus
}
#endif

class RknnFaceDetector : public QObject {
    Q_OBJECT
public:
    explicit RknnFaceDetector(QObject* parent = nullptr);
    ~RknnFaceDetector();

    // 加载模型文件
    bool loadModel(const QString& modelPath);

public slots:
    // 主调用接口：接收图像并处理
    void detect(const QImage& image);

signals:
    // 推理完成信号，返回一组检测框
    void detectionFinished(const QList<QRect>& results);

private:

    rknn_context ctx;
    int input_width = 784;
    int input_height = 784;
};

#endif // RKNNFACEDETECTOR_H
