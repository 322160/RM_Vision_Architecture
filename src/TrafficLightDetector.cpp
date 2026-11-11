#include "TrafficLightDetector.h"
#include <opencv2/dnn.hpp>
#include <algorithm>
#include <string>
using namespace cv;
using namespace std;

// 滑动条窗口名
static const char* TB_WIN = "HSV 调参";

// ==================== 四种颜色的默认阈值 ====================
// H 范围 0~180（OpenCV 的色相是 0-180，）
// 红色跨 0 度：h_min=170 > h_max=10，表示 [170,180] 和 [0,10] 两段都要
TrafficLightDetector::ColorCfg TrafficLightDetector::colors_[TrafficLightDetector::COLOR_COUNT] = {
    // name      h_min h_max  s_min s_max  v_min v_max  v_mean   draw(BGR)
    // v_min 取得低（60）：灯条是"亮核 + 暗边"的渐变，V 卡太高就只剩几像素宽的细核，
    //                    形态学一开一闭会把它打碎，整根灯条反而检不出来。
    // v_mean 才是过滤反光的开关：整块连通域的平均亮度低于它就算反光
    //                    （实测灯条平均 V 在 130 以上，车身/地面反光只有 70~100）。
    // 注：实拍灯条偏橙（H 12~19 正好落在红/黄之间的空洞），所以黄的下限从 20 收到 12。
    { "RED",      170,  10,    60,  255,    60,  255,   110,   Scalar(0, 255, 0)   },  // 红色目标用【绿框】
    { "YELLOW",    12,  35,    60,  255,    60,  255,   120,   Scalar(0, 255, 255) },
    { "GREEN",     40,  85,    60,  255,    60,  255,    90,   Scalar(0, 255, 0)   },
    { "BLUE",     100, 130,    60,  255,    60,  255,   110,   Scalar(0, 255, 255) }   // 蓝色目标用【黄框】
};

int  TrafficLightDetector::cur_[6] = { 0, 0, 0, 0, 0, 0 };
int  TrafficLightDetector::sel_ = 0;
bool TrafficLightDetector::syncing_ = false;

TrafficLightDetector::TrafficLightDetector()//这是构造函数。在这里给 private 变量赋初值
{
}

// ==================== 滑动条回调 ====================
// 滑动条改的是 cur_，这里把 cur_ 写回"当前正在调的那个颜色"
void TrafficLightDetector::onSlider(int, void*) {
    if (syncing_) return;                 
    ColorCfg& c = colors_[sel_];
    c.h_min = cur_[0];
    c.h_max = cur_[1];
    c.s_min = cur_[2];
    c.s_max = cur_[3];
    c.v_min = cur_[4];
    c.v_max = cur_[5];
}

// COLOR 滑动条：切换正在调的颜色，并把该颜色的阈值同步到滑动条位置
void TrafficLightDetector::onColor(int pos, void*) {
    if (pos < 0 || pos >= COLOR_COUNT) return;
    sel_ = pos;

    const ColorCfg& c = colors_[pos];
    syncing_ = true;

    // 先改 cur_ 再 setTrackbarPos：位置没变时 setTrackbarPos 不触发回调，
    // 但 cur_ 已经是新颜色的值，之后拖滑动条不会用错基准
    cur_[0] = c.h_min;  cur_[1] = c.h_max;
    cur_[2] = c.s_min;  cur_[3] = c.s_max;
    cur_[4] = c.v_min;  cur_[5] = c.v_max;

    setTrackbarPos("H_MIN", TB_WIN, cur_[0]);
    setTrackbarPos("H_MAX", TB_WIN, cur_[1]);
    setTrackbarPos("S_MIN", TB_WIN, cur_[2]);
    setTrackbarPos("S_MAX", TB_WIN, cur_[3]);
    setTrackbarPos("V_MIN", TB_WIN, cur_[4]);
    setTrackbarPos("V_MAX", TB_WIN, cur_[5]);

    syncing_ = false;
    cout << "当前调参颜色: " << colors_[sel_].name
         << "  (H " << cur_[0] << "~" << cur_[1]
         << ", S " << cur_[2] << "~" << cur_[3]
         << ", V " << cur_[4] << "~" << cur_[5] << ")" << endl;
}

// ==================== UI界面 ====================
void TrafficLightDetector::initTrackbars() {
    namedWindow(TB_WIN, WINDOW_NORMAL);
    resizeWindow(TB_WIN, 460, 340);

    // 6 个阈值滑动条（都绑到 cur_，具体改哪个颜色由下面的 COLOR 决定）
    createTrackbar("H_MIN", TB_WIN, &cur_[0], 180, onSlider);
    createTrackbar("H_MAX", TB_WIN, &cur_[1], 180, onSlider);
    createTrackbar("S_MIN", TB_WIN, &cur_[2], 255, onSlider);
    createTrackbar("S_MAX", TB_WIN, &cur_[3], 255, onSlider);
    createTrackbar("V_MIN", TB_WIN, &cur_[4], 255, onSlider);
    createTrackbar("V_MAX", TB_WIN, &cur_[5], 255, onSlider);

    // 颜色选择：0=RED 1=YELLOW 2=GREEN 3=BLUE
    // 创建时回调自动跑一次，把 RED 的阈值填进滑动条
    createTrackbar("COLOR 0RED 1YEL 2GRN 3BLU", TB_WIN, &sel_, COLOR_COUNT - 1, onColor);
}

// ==================== 核心处理函数 ====================
// 输入：一张图像（Mat& frame）
// 过程：转 HSV -> 对每种颜色分别 inRange 提色 -> 形态学去噪 -> 找轮廓 -> NMS 去重 -> 用该颜色画框
// 返回：所有颜色都画好框的图片
cv::Mat TrafficLightDetector::detect(Mat& frame)
{
    Mat hsv;
    cvtColor(frame, hsv, COLOR_BGR2HSV);

    // 3x3 核：装甲板灯条常常只有 4~13 像素宽，5x5 的开运算会把细灯条整根腐蚀掉
    Mat kernel = getStructuringElement(MORPH_RECT, Size(3, 3));

    // 面积阈值随图像大小自适应：小图上灯条可能只有几十像素，
    // 固定 300 会把灯条全过滤掉；大图上又容易放进噪点。
    const double min_area = std::max(30.0, 0.00015 * frame.cols * frame.rows);

    // 统计每种颜色各检到几个，显示在画面左下角
    string stat;

    for (int i = 0; i < COLOR_COUNT; i++) {
        const ColorCfg& c = colors_[i];

        // ---- 1. 按该颜色的 HSV 范围生成掩膜 ----
        Mat mask;
        if (c.h_min > c.h_max) {
            // 红色跨 0 度：两段取并集
            Mat m1, m2;
            inRange(hsv, Scalar(c.h_min, c.s_min, c.v_min), Scalar(180,     c.s_max, c.v_max), m1);
            inRange(hsv, Scalar(0,       c.s_min, c.v_min), Scalar(c.h_max, c.s_max, c.v_max), m2);
            bitwise_or(m1, m2, mask);
        } else {
            inRange(hsv, Scalar(c.h_min, c.s_min, c.v_min),
                          Scalar(c.h_max, c.s_max, c.v_max), mask);
        }

        // ---- 2. 降噪 ----
        morphologyEx(mask, mask, MORPH_OPEN,  kernel);
        morphologyEx(mask, mask, MORPH_CLOSE, kernel);

        // ---- 3. 找轮廓 ----
        vector<vector<Point>> contours;
        findContours(mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        vector<Rect>  boxes;
        vector<float> confidences;

        for (size_t k = 0; k < contours.size(); k++) {
            // 用「掩膜内的实际像素数」当面积：contourArea 算的是多边形面积，
            // 细长且倾斜的灯条会比真实像素少一大半，容易被面积阈值误杀。
            Mat blob = Mat::zeros(mask.size(), CV_8U);
            drawContours(blob, contours, (int)k, Scalar(255), FILLED);
            bitwise_and(blob, mask, blob);   // 只保留掩膜内的像素，别把轮廓里的暗洞算进去

            if (countNonZero(blob) <= min_area) continue;   // 太小的当噪点扔掉

            // 算这块连通域的平均亮度：灯条是自发光的（平均 V 约 140~250），
            // 车身、地面上的同色反光只有 70~100，用 c.v_mean 把反光一刀切掉。
            if (mean(hsv, blob)[2] < c.v_mean) continue;

            boxes.push_back(boundingRect(contours[k]));
            confidences.push_back(1.0f);
        }

        // ---- 4. 同色内部 NMS 去重 ----
        vector<int> indices;
        if (!boxes.empty()) {
            dnn::NMSBoxes(boxes, confidences, 0.0f, 0.3f, indices);
        }

        // ---- 5. 用该颜色画框 ----
        for (int idx : indices) {
            rectangle(frame, boxes[idx], c.draw, 2);   // 只画框，不写颜色名
        }

        stat += c.name.substr(0, 1) + ":" + to_string(indices.size()) + "  ";
    }

    // 左下角显示各颜色检出数量，例如 R:1  Y:0  G:2  B:0
    putText(frame, stat, Point(15, frame.rows - 15),
            FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255, 255, 255), 2);

    return frame;
}
