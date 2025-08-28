#include "rknnfacedetector.h"
#include <QFile>
#include <QDebug>
#include <algorithm>

RknnFaceDetector::RknnFaceDetector(QObject* parent) : QObject(parent), ctx(0) {}

RknnFaceDetector::~RknnFaceDetector() {
    if (ctx) {
        rknn_destroy(ctx);
    }
}

bool RknnFaceDetector::loadModel(const QString& modelPath) {
    QFile file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open RKNN model:" << modelPath;
        return false;
    }

    QByteArray modelData = file.readAll();
    file.close();

    int ret = rknn_init(&ctx, modelData.data(), modelData.size(), 0, nullptr);
    if (ret != RKNN_SUCC) {
        qWarning() << "rknn_init failed:" << ret;
        return false;
    }

    return true;
}

void RknnFaceDetector::detect(const QImage& image) {
    if (image.width() != input_width || image.height() != input_height || image.format() != QImage::Format_RGB888) {
        qWarning() << "Invalid image format or size";
        emit detectionFinished({});
        return;
    }

    // 构造输入
    rknn_input input;
    memset(&input, 0, sizeof(input));
    input.index = 0;
    input.type = RKNN_TENSOR_UINT8;
    input.size = input_width * input_height * 3;
    input.fmt = RKNN_TENSOR_NHWC;
    input.pass_through = 0;
    input.buf = (void*)image.bits();

    if (rknn_inputs_set(ctx, 1, &input) != RKNN_SUCC) {
        qWarning() << "rknn_inputs_set failed";
        emit detectionFinished({});
        return;
    }

    if (rknn_run(ctx, nullptr) != RKNN_SUCC) {
        qWarning() << "rknn_run failed";
        emit detectionFinished({});
        return;
    }

    // 查询输出数量
    const int max_outputs = 14;
    rknn_output outputs[max_outputs];
    memset(outputs, 0, sizeof(outputs));

    int real_output_num = 0;

    for (int i = 0; i < max_outputs; ++i) {
        rknn_tensor_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.index = i;
        if (rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr)) != RKNN_SUCC) {
            break; // query 失败说明已经没有更多输出
        }

        outputs[i].want_float = 1;
        real_output_num++;
    }

    if (real_output_num == 0) {
        qWarning() << "Failed to query any output tensor";
        emit detectionFinished({});
        return;
    }

    if (rknn_outputs_get(ctx, real_output_num, outputs, nullptr) != RKNN_SUCC) {
        qWarning() << "rknn_outputs_get failed";
        emit detectionFinished({});
        return;
    }

#ifdef VERBOSE
    // 可选：打印输出 tensor 属性信息（调试用）
    for (int i = 0; i < real_output_num; ++i) {
        rknn_tensor_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.index = i;
        if (rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &attr, sizeof(attr)) == RKNN_SUCC) {
            qDebug() << "Output" << i << ":" << attr.name << attr.n_elems << attr.size;
        }
    }
#endif

    QList<QRect> results;
    const float threshold = 0.5f;

    for (int stage = 0; stage < 5; ++stage) {
        int score_idx = stage;
        int bbox_idx  = stage + 5;
        // int kps_idx   = stage + 10; // 如需关键点，可启用

        float* scores = static_cast<float*>(outputs[score_idx].buf);
        float* bboxes = static_cast<float*>(outputs[bbox_idx].buf);
        int num_anchors = outputs[score_idx].size / sizeof(float);

        for (int i = 0; i < num_anchors; ++i) {
            float score = scores[i];
            qDebug() << "Score:" << score;
            if (score < threshold)
                continue;

            float x1 = bboxes[i * 4 + 0];
            float y1 = bboxes[i * 4 + 1];
            float x2 = bboxes[i * 4 + 2];
            float y2 = bboxes[i * 4 + 3];

            int left   = std::max(0, std::min((int)x1, input_width));
            int top    = std::max(0, std::min((int)y1, input_height));
            int right  = std::max(0, std::min((int)x2, input_width));
            int bottom = std::max(0, std::min((int)y2, input_height));

            results.append(QRect(left, top, right - left, bottom - top));
        }
    }

    rknn_outputs_release(ctx, real_output_num, outputs);
    emit detectionFinished(results);

    for (const QRect& rect : results) {
        qDebug() << "Face detected at:"
                 << "x =" << rect.x()
                 << "y =" << rect.y()
                 << "width =" << rect.width()
                 << "height =" << rect.height();
    }
}
