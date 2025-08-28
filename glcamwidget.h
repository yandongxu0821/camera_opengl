#ifndef GLCAMWIDGET_H
#define GLCAMWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions_ES2>
#include <QOpenGLShaderProgram>
#include <QImage>
#include <QMutex>
#include "myconfig.h"

class GLCamWidget : public QOpenGLWidget, protected QOpenGLFunctions_ES2
{
    Q_OBJECT
public:
    explicit GLCamWidget(QWidget *parent = nullptr);
    ~GLCamWidget() override;

public slots:
    void onNewFrame(const QImage &img);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QImage currentImage;
    QMutex mutex;
    QOpenGLShaderProgram shaderProgram;
    GLuint textureId = 0;
    void updateTexture();

    GLuint vbo = 0;
    GLint attrPos = -1;
    GLint attrTex = -1;
    GLint uniformTex = -1;

};

#endif // GLCAMWIDGET_H
