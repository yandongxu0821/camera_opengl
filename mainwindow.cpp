#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "cameracapturethread.h"
#include "glcamwidget.h"
#include "rknnfacedetector.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      cameraThread(new CameraCaptureThread(this)),
      glCamWidget(new GLCamWidget(this))
{
    setCentralWidget(glCamWidget);
    // ui->setupUi(this);
    connect(cameraThread, &CameraCaptureThread::frameReadyForDisplay, glCamWidget, &GLCamWidget::onNewFrame);
    // 初始化
    auto* detector = new RknnFaceDetector(this);
    if (!detector->loadModel("/userdata/movenet_lightning.rknn")) {
        qWarning() << "模型加载失败！";
        return;
    }

    connect(cameraThread, &CameraCaptureThread::frameReadyForInfer, detector, &RknnFaceDetector::detect);
    // connect(detector, &RknnFaceDetector::detectionFinished, this, &MainWindow::onDetectionFinished);

    cameraThread->start();
}

MainWindow::~MainWindow() { }
