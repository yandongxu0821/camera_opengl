#ifndef CAMERACAPTURETHREAD_H
#define CAMERACAPTURETHREAD_H

#include <QObject>
#include <QThread>
#include <QImage>
#include "myconfig.h"

class CameraCaptureThread : public QThread {
    Q_OBJECT
public:
    explicit CameraCaptureThread(QObject *parent = nullptr);
    ~CameraCaptureThread() override;
    void stop();

    QImage preProcessToSquare(const QImage& input, int squareSize);

signals:
    // void frameReady(const QImage &frame);
    void frameReadyForDisplay(const QImage &frame);
    void frameReadyForInfer(const QImage& img);

protected:
    void run() override;

private:
    bool running = true;
};

struct Buffer {
    void *start;
    size_t length;
};


#endif // CAMERACAPTURETHREAD_H
