#include <iostream>
#include <string>
#include <cctype>
#include <fstream>
#include <ctime>
#include <opencv2/opencv.hpp>
#include "TrafficLightDetector.h"

using namespace std;
using namespace cv;

// 默认输入：图片 / 视频 / 摄像头都行（也可以命令行传参，见 main）
static const string DEFAULT_INPUT = "test.jpg";

// ==================== 工具函数 ====================

// 按扩展名判断是不是图片
static bool isImageFile(const string& path) {
    size_t dot = path.find_last_of('.');
    if (dot == string::npos) return false;
    string ext = path.substr(dot + 1);
    for (char& c : ext) c = (char)tolower(c);
    return ext == "jpg" || ext == "jpeg" || ext == "png" ||
           ext == "bmp" || ext == "tif"  || ext == "tiff" || ext == "webp";
}

// 输入是不是摄像头编号（"0"、"1"…）
static bool isCameraIndex(const string& path) {
    if (path.empty()) return false;
    for (char c : path) if (!isdigit((unsigned char)c)) return false;
    return true;
}

// 取文件名（去掉目录和扩展名），给结果图命名用
static string fileStem(const string& path) {
    size_t slash = path.find_last_of("/\\");
    string name = (slash == string::npos) ? path : path.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    return (dot == string::npos) ? name : name.substr(0, dot);
}

// 文件是否已存在
static bool fileExists(const string& path) {
    ifstream f(path.c_str());
    return f.good();
}

// 生成不覆盖的文件名：xxx.jpg 已存在就依次试 xxx_1.jpg、xxx_2.jpg…
static string uniqueName(const string& base) {
    string name = base + ".jpg";
    if (!fileExists(name)) return name;
    for (int i = 1; i < 1000; i++) {
        string cand = base + "_" + to_string(i) + ".jpg";
        if (!fileExists(cand)) return cand;
    }
    return base + "_" + to_string((long long)time(nullptr)) + ".jpg";
}

// 单帧检测：OpenCV 传统视觉，框直接画在 frame 上
static void detectFrame(Mat& frame, TrafficLightDetector& detector) {
    frame = detector.detect(frame);
    putText(frame, "Mode: OpenCV", Point(20, 22),
            FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 1);
}

int main(int argc, char** argv) {
    // 用法：./vision_app [图片或视频路径 | 摄像头编号]
    // 不传参就用 DEFAULT_INPUT
    string input = (argc > 1) ? argv[1] : DEFAULT_INPUT;

    TrafficLightDetector detector;

    // HSV 调参滑动条（暂时注释掉，需要调参时把下一行取消注释即可）
    // detector.initTrackbars();

    namedWindow("Detector", WINDOW_NORMAL);
    resizeWindow("Detector", 800, 600);

    // ==================== 分支一：图片 ====================
    if (isImageFile(input)) {
        Mat src = imread(input);
        if (src.empty()) {
            cerr << "读不到图片: " << input << endl;
            return -1;
        }
        cout << "图片模式: " << input << "  (" << src.cols << "x" << src.rows << ")" << endl;

        bool running = true;
        while (running) {
            Mat canvas = src.clone();          // 每次重新拷贝，重复检测不会叠加框
            detectFrame(canvas, detector);

            cout << "[OpenCV] 图片处理完成" << endl;
            imshow("Detector", canvas);

            // 图片是静止的，用 waitKey(0) 停下来等按键
            // 's' 保存结果   'q'/ESC 退出
            int key = waitKey(0);
            if (key == 's' || key == 'S') {
                // 文件名带上原图名，不同图片不会互相覆盖；
                // 同一张图重复保存会自动加序号：_1、_2…
                string out = uniqueName("result_opencv_" + fileStem(input));
                imwrite(out, canvas);
                cout << ">>> 已保存: " << out << endl;
            } else if (key == 'q' || key == 27) {
                running = false;
            }
        }
        return 0;
    }

    // ==================== 分支二：视频 / 摄像头 ====================
    VideoCapture cap;
    if (isCameraIndex(input)) {
        cap.open(stoi(input));
        cout << "摄像头模式: index " << input << endl;
    } else {
        cap.open(input);
        cout << "视频模式: " << input << endl;
    }
    if (!cap.isOpened()) {
        cerr << "打不开输入源: " << input << endl;
        return -1;
    }

    Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;              // 视频放完 / 摄像头断开

        detectFrame(frame, detector);
        imshow("Detector", frame);

        // 'q'/ESC 退出
        int key = waitKey(1);
        if (key == 'q' || key == 27) {
            break;
        }
    }

    return 0;
}
