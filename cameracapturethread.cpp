#include "cameracapturethread.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <QDebug>
#include <cstring>

#define DEVICE "/dev/video11"
#define WIDTH 1056    // 4224
#define HEIGHT 784   // 3136
#define BUFFER_COUNT 4

CameraCaptureThread::CameraCaptureThread(QObject *parent) : QThread(parent) { }
CameraCaptureThread::~CameraCaptureThread() { stop(); }

void CameraCaptureThread::stop() {
    running = false;
    wait();
}

//! \brief 预处理帧尺寸为正方形
QImage CameraCaptureThread::preProcessToSquare(const QImage& input, int squareSize = HEIGHT) {
    int w = input.width();
    int h = input.height();

    // 取中间正方形区域
    int x = (w - squareSize) / 2;
    int y = (h - squareSize) / 2;
    return input.copy(x, y, squareSize, squareSize);
}

void CameraCaptureThread::run() {
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        qDebug() << "Failed to open device";
        return;
    }

    // 设置格式（多平面）
    struct v4l2_format fmt = {};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = WIDTH;
    fmt.fmt.pix_mp.height = HEIGHT;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    fmt.fmt.pix_mp.num_planes = 2;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        qDebug() << "VIDIOC_S_FMT failed:" << strerror(errno);
        close(fd);
        return;
    }

#ifdef CAMTEST
    /**** 验证实际格式 ****/
    int planesNum = fmt.fmt.pix_mp.num_planes;
    if (ioctl(fd, VIDIOC_G_FMT, &fmt) == 0) {
        qDebug() << "驱动接受格式:";
        qDebug() << " width:" << fmt.fmt.pix_mp.width << " height:" << fmt.fmt.pix_mp.height;
        qDebug() << " pixel format:" << QString::number(fmt.fmt.pix_mp.pixelformat, 16);
        qDebug() << " planes:" << fmt.fmt.pix_mp.num_planes;
        for (int i = 0; i < fmt.fmt.pix_mp.num_planes; ++i) {
            qDebug() << " plane" << i << " sizeimage:" << fmt.fmt.pix_mp.plane_fmt[i].sizeimage << " bytesperline:" << fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
        }
    }
    /********/
#endif

    // 请求缓冲区
    struct v4l2_requestbuffers req = {};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    req.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) {
        qDebug() << "VIDIOC_REQBUFS failed";
        close(fd);
        return;
    }

    Buffer buffers[BUFFER_COUNT];

    for (int i = 0; i < BUFFER_COUNT; ++i) {
        struct v4l2_buffer buf = {};
        struct v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = 1;
        buf.m.planes = planes;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            qDebug() << "VIDIOC_QUERYBUF failed";
            close(fd);
            return;
        }

        buffers[i].length = buf.m.planes[0].length;
        buffers[i].start = mmap(NULL, buf.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.planes[0].m.mem_offset);
        if (buffers[i].start == MAP_FAILED) {
            qDebug() << "mmap failed";
            close(fd);
            return;
        }

    }

    // 入队缓冲区
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        struct v4l2_buffer buf = {};
        struct v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.length = 1;
        buf.m.planes = planes;

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            qDebug() << "VIDIOC_QBUF failed";
            close(fd);
            return;
        }
    }

    // 启动采集
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        qDebug() << "VIDIOC_STREAMON failed";
        close(fd);
        return;
    }

    running = true;

    while (running) {
#ifdef CAMTEST
        /**** 测试部分，验证设备能力 ****/
        struct v4l2_capability cap = {};
        if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
            qDebug() << "Driver:" << (const char*)cap.driver << " Card:" << (const char*)cap.card;   // Driver: rkisp_v6  Card: rkisp_mainpath
        if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
            qDebug() << "支持 V4L2_CAP_VIDEO_CAPTURE_MPLANE";
        else
            qDebug() << "不支持 V4L2_CAP_VIDEO_CAPTURE_MPLANE";
        }
        /**** 结束 ****/
#endif

        struct v4l2_buffer buf = {};
        struct v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        memset(&buf, 0, sizeof(buf));
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = planes;

        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
            qDebug() << "VIDIOC_DQBUF failed";
            break;
        }

        uchar *yuv = static_cast<uchar *>(buffers[buf.index].start);

        QImage img(WIDTH, HEIGHT, QImage::Format_RGB888);
        uchar *rgb = img.bits();

        /**** NV12 → RGB888 转换 ****/
        uchar* yPlane = yuv;
        uchar* uvPlane = yuv + WIDTH * HEIGHT;

        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                int y_index = y * WIDTH + x;
                int uv_index = (y / 2) * WIDTH + (x & ~1);
                int Y = yPlane[y_index];
                int U = uvPlane[uv_index] - 128;
                int V = uvPlane[uv_index + 1] - 128;
                int R = Y + int(1.402 * V);
                int G = Y - int(0.344136 * U) - int(0.714136 * V);
                int B = Y + int(1.772 * U);

                rgb[(y * WIDTH + x) * 3 + 0] = qBound(0, R, 255);
                rgb[(y * WIDTH + x) * 3 + 1] = qBound(0, G, 255);
                rgb[(y * WIDTH + x) * 3 + 2] = qBound(0, B, 255);
            }
        }

        emit frameReadyForDisplay(img);

#ifdef RKNN
        QImage croppedImg = preProcessToSquare(img, HEIGHT);
        emit frameReadyForInfer(croppedImg);
#endif

        if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
            qDebug() << "VIDIOC_QBUF failed";
            break;
        }
    }

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        if (buffers[i].start && buffers[i].length) {
            munmap(buffers[i].start, buffers[i].length);
        }
    }
    close(fd);
}

