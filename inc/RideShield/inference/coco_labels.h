#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace RideShield::inference {

// COCO 80 类别名称 (中文)
inline constexpr std::array<std::string_view, 80> kCoco80Labels = {{
    "人", "自行车", "汽车", "摩托车", "飞机",
    "公交车", "火车", "卡车", "船", "红绿灯",
    "消火栓", "停止标志", "停车计时器", "长椅", "鸟",
    "猫", "狗", "马", "羊", "牛",
    "大象", "熊", "斑马", "长颈鹿", "背包",
    "雨伞", "手提包", "领带", "行李箱", "飞盘",
    "双板滑雪板", "单板滑雪板", "运动球", "风筝", "棒球棒",
    "棒球手套", "滑板", "冲浪板", "网球拍", "瓶子",
    "红酒杯", "杯子", "叉子", "刀", "勺子",
    "碗", "香蕉", "苹果", "三明治", "橙子",
    "西兰花", "胡萝卜", "热狗", "披萨", "甜甜圈",
    "蛋糕", "椅子", "沙发", "盆栽", "床",
    "餐桌", "马桶", "电视", "笔记本电脑", "鼠标",
    "遥控器", "键盘", "手机", "微波炉", "烤箱",
    "烤面包机", "水槽", "冰箱", "书", "时钟",
    "花瓶", "剪刀", "泰迪熊", "吹风机", "牙刷",
}};

inline auto coco80_label(std::size_t class_id) -> std::string_view {
    if (class_id < kCoco80Labels.size()) {
        return kCoco80Labels[class_id];
    }
    return "unknown";
}

// COCO 80 类别名称 (英文, 用于 OpenCV putText 等不支持 Unicode 的场景)
inline constexpr std::array<std::string_view, 80> kCoco80LabelsEn = {{
    "person", "bicycle", "car", "motorcycle", "airplane",
    "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird",
    "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack",
    "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat",
    "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle",
    "wine glass", "cup", "fork", "knife", "spoon",
    "bowl", "banana", "apple", "sandwich", "orange",
    "broccoli", "carrot", "hot dog", "pizza", "donut",
    "cake", "chair", "couch", "potted plant", "bed",
    "dining table", "toilet", "tv", "laptop", "mouse",
    "remote", "keyboard", "cell phone", "microwave", "oven",
    "toaster", "sink", "refrigerator", "book", "clock",
    "vase", "scissors", "teddy bear", "hair drier", "toothbrush",
}};

inline auto coco80_label_en(std::size_t class_id) -> std::string_view {
    if (class_id < kCoco80LabelsEn.size()) {
        return kCoco80LabelsEn[class_id];
    }
    return "unknown";
}

}  // namespace RideShield::inference
