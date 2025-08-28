#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "cameracapturethread.h"
#include "myconfig.h"
#include "glcamwidget.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    CameraCaptureThread *cameraThread;
    GLCamWidget *glCamWidget;
};

#endif // MAINWINDOW_H
