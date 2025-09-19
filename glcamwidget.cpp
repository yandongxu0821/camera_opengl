#include "glcamwidget.h"
#include <QOpenGLTexture>
#include <QDebug>

GLCamWidget::GLCamWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumSize(640, 480);
}

GLCamWidget::~GLCamWidget() {
    makeCurrent();
    if (textureId) glDeleteTextures(1, &textureId);
    if (vbo) glDeleteBuffers(1, &vbo);
    doneCurrent();
}

void GLCamWidget::onNewFrame(const QImage &img) {
    QMutexLocker locker(&mutex);
    currentImage = img.copy();
    update();
}

void GLCamWidget::initializeGL() {
    initializeOpenGLFunctions();

    // 顶点着色器和片元着色器
    const char *vsrc =
        "attribute vec2 position;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "    vTexCoord = texCoord;\n"
        "}";
    const char *fsrc =
        "precision mediump float;\n"
        "varying vec2 vTexCoord;\n"
        "uniform sampler2D textureSampler;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(textureSampler, vTexCoord);\n"
        "}";

    shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
    shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
    shaderProgram.link();

    attrPos = shaderProgram.attributeLocation("position");
    attrTex = shaderProgram.attributeLocation("texCoord");
    uniformTex = shaderProgram.uniformLocation("textureSampler");

    static const GLfloat vertexData[] = {
        // 位置   // 纹理坐标
        -1, -1,   0, 1,
         1, -1,   1, 1,
        -1,  1,   0, 0,
         1,  1,   1, 0
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void GLCamWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void GLCamWidget::paintGL() {
    QMutexLocker locker(&mutex);
    if (currentImage.isNull()) return;

    glClear(GL_COLOR_BUFFER_BIT);

    QImage glImg = currentImage.convertToFormat(QImage::Format_RGBA8888);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 glImg.width(), glImg.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, glImg.bits());

    shaderProgram.bind();
    shaderProgram.setUniformValue(uniformTex, 0);

    // === 计算自适应顶点 ===
    int imgW = glImg.width();
    int imgH = glImg.height();
    int winW = width();
    int winH = height();

    float imgAspect = (float)imgW / imgH;
    float winAspect = (float)winW / winH;

    float xScale = 1.0f;
    float yScale = 1.0f;

    if (imgAspect > winAspect) {
        // 图片宽，窗口相对窄 => 裁剪上下
        yScale = winAspect / imgAspect;
    } else {
        // 图片高，窗口相对宽 => 裁剪左右
        xScale = imgAspect / winAspect;
    }

    GLfloat vertexData[] = {
        -xScale, -yScale,  0.0f, 1.0f,
         xScale, -yScale,  1.0f, 1.0f,
        -xScale,  yScale,  0.0f, 0.0f,
         xScale,  yScale,  1.0f, 0.0f
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(attrPos);
    glEnableVertexAttribArray(attrTex);
    glVertexAttribPointer(attrPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
    glVertexAttribPointer(attrTex, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(attrPos);
    glDisableVertexAttribArray(attrTex);
    shaderProgram.release();
}
