#pragma once
#include <opencv2/opencv.hpp>
#include <string>

//定义一个类：红绿灯检测器
//一次 detect 同时检测 红 / 黄 / 绿 / 蓝 四种颜色
class TrafficLightDetector
{
public://对外的接口
    TrafficLightDetector();
    ////传TrafficLightDetector() 是构造函数，detect 是主函数。外面的 main.cpp 只能调用这里的函数。入一帧图像，返回处理后的图像
    cv::Mat detect(cv::Mat &frame);
    void initTrackbars();//初始化滑动条

    static const int COLOR_COUNT = 4;   // 支持的颜色数量

private://内部私有数据  (加入UI)
    struct ColorCfg {
        std::string name;      // 颜色名，画在框上
        int h_min, h_max;      // 红色 h_min > h_max 表示跨 0 度：[h_min,180] ∪ [0,h_max]
        int s_min, s_max;
        int v_min, v_max;
        int v_mean;            // 该连通域的平均亮度下限：灯条自发光所以很亮，
                               // 车身/地面上的同色反光偏暗，靠这个把反光滤掉
        cv::Scalar draw;       // 画框颜色（BGR）
    };

    static ColorCfg colors_[COLOR_COUNT];  // 每种颜色一套阈值
    static int  cur_[6];                   // 滑动条绑定的值（滑哪个改哪个）
    static int  sel_;                      // 当前正在调的颜色下标
    static bool syncing_;                  // 切换颜色时屏蔽滑动条回调

    static void onSlider(int, void*);      // 滑动条回调：把值写回当前颜色
    static void onColor(int, void*);       // 切换颜色：把该颜色的阈值同步到滑动条
};
