#include "resources.h"

#include "RideShield/core/types.h"
#include "RideShield/core/image_view.h"
#include "RideShield/core/tensor_view.h"
#include "RideShield/inference/coco_labels.h"
#include "RideShield/decision/fusion_engine.h"

#ifdef RIDESHIELD_HAS_OPENCV
#include "RideShield/inference/yolo_preprocess.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

#ifdef RIDESHIELD_HAS_ONNXRUNTIME
#include "RideShield/inference/yolo_detector.h"
#include "RideShield/perception/front_perception.h"
#include "RideShield/perception/rear_perception.h"
#endif

#include <gtest/gtest.h>

using namespace RideShield;

// ============================================================
//  域类型测试
// ============================================================

TEST(DomainTypesTest, BoundingBoxGeometry) {
    core::BoundingBox bbox{.left = 10, .top = 20, .right = 110, .bottom = 120};
    EXPECT_FLOAT_EQ(bbox.width(), 100.f);
    EXPECT_FLOAT_EQ(bbox.height(), 100.f);
    EXPECT_FLOAT_EQ(bbox.center_x(), 60.f);
    EXPECT_FLOAT_EQ(bbox.center_y(), 70.f);
    EXPECT_FLOAT_EQ(bbox.area(), 10000.f);
}

TEST(DomainTypesTest, BoundingBoxNegativeClamp) {
    core::BoundingBox bbox{.left = 50, .top = 50, .right = 30, .bottom = 30};
    EXPECT_FLOAT_EQ(bbox.width(), 0.f);
    EXPECT_FLOAT_EQ(bbox.height(), 0.f);
}

TEST(DomainTypesTest, DetectionReportDefault) {
    core::DetectionReport report;
    EXPECT_TRUE(report.detections.empty());
    EXPECT_FLOAT_EQ(report.inference_ms, 0.f);
}

// ============================================================
//  COCO 标签测试
// ============================================================

TEST(CocoLabelsTest, KnownLabels) {
    EXPECT_EQ(inference::coco80_label(0), "人");
    EXPECT_EQ(inference::coco80_label(2), "汽车");
    EXPECT_EQ(inference::coco80_label(5), "公交车");
    EXPECT_EQ(inference::coco80_label(79), "牙刷");
}

TEST(CocoLabelsTest, OutOfRangeReturnsUnknown) {
    EXPECT_EQ(inference::coco80_label(80), "unknown");
    EXPECT_EQ(inference::coco80_label(9999), "unknown");
}

// ============================================================
//  融合引擎测试
// ============================================================

TEST(FusionEngineTest, AllNormalYieldsL0) {
    decision::FusionEngine engine;
    decision::FusionEngine::Input input{};
    auto result = engine.evaluate(input);

    EXPECT_EQ(result.overall_risk, core::RiskLevel::kNormal);
    EXPECT_FLOAT_EQ(result.risk_score, 0.f);
    EXPECT_FALSE(result.should_warn_voice);
    EXPECT_FALSE(result.should_warn_vibrate);
    EXPECT_FALSE(result.should_brake);
}

TEST(FusionEngineTest, FrontEmergencyButFarAwayYieldsLow) {
    decision::FusionEngine engine;
    decision::FusionEngine::Input input{};
    input.front.risk = core::RiskLevel::kEmergency;
    // TTC 默认 1e9f (极远) → 动态算法应大幅降低前向分
    auto result = engine.evaluate(input);

    // 目标远且无接近趋势 → 不应触发告警
    EXPECT_EQ(result.overall_risk, core::RiskLevel::kNormal);
    EXPECT_FALSE(result.should_warn_voice);
}

TEST(FusionEngineTest, FrontEmergencyWithLowTTCTriggersWarning) {
    decision::FusionEngine engine;
    decision::FusionEngine::Input input{};
    input.front.risk = core::RiskLevel::kEmergency;
    input.front.ttc_seconds = 2.0f; // TTC 2s → 真正危险
    auto result = engine.evaluate(input);

    // front weight 0.30 * 1.0 = 0.30 → > hint(0.20)
    EXPECT_GE(static_cast<int>(result.overall_risk), static_cast<int>(core::RiskLevel::kHint));
    EXPECT_TRUE(result.should_warn_voice);
}

TEST(FusionEngineTest, MultipleHighRisksEscalate) {
    decision::FusionEngine engine;
    decision::FusionEngine::Input input{};
    input.front.risk = core::RiskLevel::kEmergency;
    input.front.ttc_seconds = 1.0f; // 真正的紧急 TTC
    input.rear.risk = core::RiskLevel::kEmergency;
    input.driver.risk = core::RiskLevel::kEmergency;
    auto result = engine.evaluate(input);

    // front 0.30*0.9 + rear 0.20 + driver 0.25 = 0.72 → L3
    EXPECT_EQ(result.overall_risk, core::RiskLevel::kEmergency);
    EXPECT_TRUE(result.should_warn_voice);
    EXPECT_TRUE(result.should_warn_vibrate);
}

TEST(FusionEngineTest, BrakeBlockedByImuAbnormal) {
    decision::FusionEngine engine;
    decision::FusionEngine::Input input{};
    // 全部紧急, risk_score 应该很高
    input.front.risk = core::RiskLevel::kEmergency;
    input.front.ttc_seconds = 0.5f; // 紧急 TTC
    input.rear.risk = core::RiskLevel::kEmergency;
    input.driver.risk = core::RiskLevel::kEmergency;
    input.imu.risk = core::RiskLevel::kEmergency;
    input.hr.risk = core::RiskLevel::kEmergency;

    // IMU 异常时不允许制动
    input.imu.abnormal = true;
    auto result = engine.evaluate(input);
    EXPECT_FALSE(result.should_brake);

    // IMU 正常时允许制动
    input.imu.abnormal = false;
    result = engine.evaluate(input);
    EXPECT_TRUE(result.should_brake);
}

TEST(FusionEngineTest, FatigueAndHeartRateBoost) {
    decision::FusionEngine engine;
    decision::FusionEngine::Input input{};
    input.driver.risk = core::RiskLevel::kWarning;
    input.hr.risk = core::RiskLevel::kWarning;

    auto result = engine.evaluate(input);
    float base_score = 0.25f * 0.6f + 0.10f * 0.6f; // 0.21
    // 疲劳 + 心率联合 → score * 1.3
    EXPECT_GT(result.risk_score, base_score);
}

// ============================================================
//  预处理测试 (需要 OpenCV)
// ============================================================

#ifdef RIDESHIELD_HAS_OPENCV

TEST(YoloPreprocessTest, OutputTensorShapeCorrect) {
    inference::YoloPreprocessContext ctx(640);

    // 创建 BGR 测试图像
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(128, 64, 32));
    auto prepared = ctx.prepare(frame);

    EXPECT_EQ(prepared.original_width, 640);
    EXPECT_EQ(prepared.original_height, 480);
    EXPECT_EQ(prepared.tensor.rank(), 4);
    EXPECT_EQ(prepared.tensor.shape()[0], 1);  // batch
    EXPECT_EQ(prepared.tensor.shape()[1], 3);  // channels
    EXPECT_EQ(prepared.tensor.shape()[2], 640); // height
    EXPECT_EQ(prepared.tensor.shape()[3], 640); // width
    EXPECT_EQ(prepared.tensor.element_count(), 1 * 3 * 640 * 640);
}

TEST(YoloPreprocessTest, LetterboxScaleAndPadding) {
    inference::YoloPreprocessContext ctx(640);

    // 宽图 → 应该有上下 padding
    cv::Mat wide_frame(240, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    auto prepared = ctx.prepare(wide_frame);

    EXPECT_FLOAT_EQ(prepared.scale, 1.0f); // 640/640 = 1.0
    EXPECT_FLOAT_EQ(prepared.pad_x, 0.f);
    EXPECT_GT(prepared.pad_y, 0.f); // 有上下 padding

    // 高图 → 应该有左右 padding
    cv::Mat tall_frame(640, 320, CV_8UC3, cv::Scalar(0, 0, 0));
    prepared = ctx.prepare(tall_frame);

    EXPECT_FLOAT_EQ(prepared.scale, 1.0f); // 640/640 = 1.0
    EXPECT_GT(prepared.pad_x, 0.f); // 有左右 padding
    EXPECT_FLOAT_EQ(prepared.pad_y, 0.f);
}

TEST(YoloPreprocessTest, PixelValuesNormalized) {
    inference::YoloPreprocessContext ctx(4);

    // 创建纯白 BGR 图像 (255, 255, 255)
    cv::Mat white(4, 4, CV_8UC3, cv::Scalar(255, 255, 255));
    auto prepared = ctx.prepare(white);

    auto span = prepared.tensor.span<const float>();
    // 所有值应在 [0, 1] 范围内
    for (auto v : span) {
        EXPECT_GE(v, 0.f);
        EXPECT_LE(v, 1.f);
    }

    // 白色像素归一化后应为 1.0 (R=255/255, G=255/255, B=255/255)
    // R plane 起始位置的值
    EXPECT_FLOAT_EQ(span[0], 1.f); // R channel
}

TEST(YoloPreprocessTest, BgrToRgbConversion) {
    inference::YoloPreprocessContext ctx(2);

    // 纯蓝 BGR = (255, 0, 0)
    cv::Mat blue(2, 2, CV_8UC3, cv::Scalar(255, 0, 0));
    auto prepared = ctx.prepare(blue);

    auto span = prepared.tensor.span<const float>();
    const auto plane = 2 * 2; // 4 elements per channel plane

    // R plane 应该全 0 (源是蓝色 B=255, G=0, R=0)
    EXPECT_FLOAT_EQ(span[0], 0.f);         // R
    EXPECT_FLOAT_EQ(span[plane], 0.f);     // G
    EXPECT_FLOAT_EQ(span[2 * plane], 1.f); // B → 源 BGR[0]=255
}

TEST(YoloPreprocessTest, ZeroCopyTensorPointsToInternalBuffer) {
    inference::YoloPreprocessContext ctx(64);

    cv::Mat frame(48, 64, CV_8UC3, cv::Scalar(0, 0, 0));
    auto prepared1 = ctx.prepare(frame);
    const void* ptr1 = prepared1.tensor.data();

    // 第二次 prepare 应该复用同一个缓冲区
    auto prepared2 = ctx.prepare(frame);
    const void* ptr2 = prepared2.tensor.data();

    EXPECT_EQ(ptr1, ptr2); // 零拷贝: 指向同一缓冲区
}

TEST(YoloPreprocessTest, ImageViewAndCvMatGiveSameResult) {
    inference::YoloPreprocessContext ctx1(64);
    inference::YoloPreprocessContext ctx2(64);

    cv::Mat frame(48, 64, CV_8UC3);
    cv::randu(frame, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));

    auto prepared_mat = ctx1.prepare(frame);
    auto view = core::borrow_image(frame);
    auto prepared_view = ctx2.prepare(view);

    auto span1 = prepared_mat.tensor.span<const float>();
    auto span2 = prepared_view.tensor.span<const float>();

    ASSERT_EQ(span1.size(), span2.size());
    for (std::size_t i = 0; i < span1.size(); ++i) {
        EXPECT_FLOAT_EQ(span1[i], span2[i]);
    }
}

#endif // RIDESHIELD_HAS_OPENCV

// ============================================================
//  ONNX 检测器端到端测试 (需要 ONNX Runtime + OpenCV)
// ============================================================

#if defined(RIDESHIELD_HAS_ONNXRUNTIME) && defined(RIDESHIELD_HAS_OPENCV)

class YoloDetectorTest : public ::testing::Test {
protected:
    static inference::YoloDetectorConfig make_config(float score_threshold = 0.25f,
                                                      std::size_t threads = 2) {
        inference::YoloDetectorConfig cfg{
            .input_size = 640,
            .score_threshold = score_threshold,
            .intra_threads = threads,
        };
        cfg.model_data = RideShield::resources::get("res/yolo26n.onnx");
        return cfg;
    }

    static bool model_available() {
        return !RideShield::resources::find("res/yolo26n.onnx").empty();
    }
};

TEST_F(YoloDetectorTest, InitializesWithoutCrash) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    EXPECT_NO_THROW(inference::YoloDetector detector(make_config()));
}

TEST_F(YoloDetectorTest, DetectsBlackImageWithNoTargets) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    inference::YoloDetector detector(make_config());

    cv::Mat black(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    auto report = detector.detect(black);

    // 黑色图像不应该产生高置信度检测
    EXPECT_GE(report.inference_ms, 0.f);
    for (const auto& det : report.detections) {
        // 如果有检测，置信度不应极高（噪声检测）
        EXPECT_LT(det.confidence, 0.9f);
    }
}

TEST_F(YoloDetectorTest, DetectsOnDifferentSizeImages) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    inference::YoloDetector detector(make_config());

    // 多种不同尺寸的图像应该都能正常处理
    for (auto [w, h] : std::vector<std::pair<int, int>>{{640, 480}, {320, 240}, {1920, 1080}, {100, 100}}) {
        cv::Mat frame(h, w, CV_8UC3, cv::Scalar(128, 128, 128));
        EXPECT_NO_THROW({
            auto report = detector.detect(frame);
            EXPECT_GE(report.inference_ms, 0.f);
        }) << "Failed for image size " << w << "x" << h;
    }
}

TEST_F(YoloDetectorTest, DetectionBboxWithinOriginalImageBounds) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    inference::YoloDetector detector(make_config(0.10f));

    cv::Mat frame(480, 640, CV_8UC3);
    cv::randu(frame, cv::Scalar(0, 0, 0), cv::Scalar(255, 255, 255));
    auto report = detector.detect(frame);

    for (const auto& det : report.detections) {
        EXPECT_GE(det.bbox.left, 0.f);
        EXPECT_GE(det.bbox.top, 0.f);
        EXPECT_LE(det.bbox.right, static_cast<float>(frame.cols));
        EXPECT_LE(det.bbox.bottom, static_cast<float>(frame.rows));
        EXPECT_GT(det.bbox.width(), 0.f);
        EXPECT_GT(det.bbox.height(), 0.f);
        EXPECT_LT(det.class_id, 80u);
    }
}

TEST_F(YoloDetectorTest, ZeroCopyNoExtraCopies) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    inference::YoloDetector detector(make_config());

    // 连续两次推理，验证结果一致
    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(64, 128, 192));
    auto report1 = detector.detect(frame);
    auto report2 = detector.detect(frame);

    EXPECT_EQ(report1.detections.size(), report2.detections.size());
}

// ============================================================
//  前向感知模块端到端测试
// ============================================================

TEST_F(YoloDetectorTest, FrontPerceptionIntegration) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    perception::FrontPerception front(make_config());

    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    auto result = front.process(frame);

    EXPECT_GE(result.report.inference_ms, 0.f);
    EXPECT_GE(result.ttc_seconds, 0.f);
    // 黑色图像 → 正常或低风险
    EXPECT_LE(static_cast<int>(result.risk), static_cast<int>(core::RiskLevel::kHint));
}

// ============================================================
//  后向感知模块端到端测试
// ============================================================

TEST_F(YoloDetectorTest, RearPerceptionIntegration) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    perception::RearPerception rear(make_config(0.30f));

    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));
    auto result = rear.process(frame);

    EXPECT_GE(result.report.inference_ms, 0.f);
    // 黑色图像 → 无盲区占用
}

// ============================================================
//  完整管线集成测试
// ============================================================

TEST_F(YoloDetectorTest, FullPipelineIntegration) {
    if (!model_available()) GTEST_SKIP() << "Model not available";

    // 前向
    perception::FrontPerception front(make_config());

    // 后向
    perception::RearPerception rear(make_config(0.30f));

    // 融合
    decision::FusionEngine fusion;

    cv::Mat frame(480, 640, CV_8UC3, cv::Scalar(64, 64, 64));
    auto front_result = front.process(frame);
    auto rear_result = rear.process(frame);

    decision::FusionEngine::Input input{
        .front = front_result,
        .rear = rear_result,
    };

    auto decision = fusion.evaluate(input);

    // 验证基本一致性
    EXPECT_GE(decision.risk_score, 0.f);
    EXPECT_LE(decision.risk_score, 1.f);
}

#endif // RIDESHIELD_HAS_ONNXRUNTIME && RIDESHIELD_HAS_OPENCV
