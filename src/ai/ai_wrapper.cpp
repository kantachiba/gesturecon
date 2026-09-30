/*
 * ai_wrapper.cpp
 * Edge Impulse C++ → μT-Kernel C ブリッジ
 *
 * コンパイル前に edge-impulse-sdk/ と model-parameters/ を
 * このファイルと同じディレクトリに配置すること。
 */

/* Edge Impulse SDK のメインヘッダ（C++ ライブラリ展開後に有効になる）*/
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include "ai_wrapper.h"

/* TFLite Micro 推論アリーナ（静的確保・malloc 不使用）*/
static uint8_t tensor_arena[EI_TENSOR_ARENA_SIZE];

/* 推論結果確信度の閾値 */
#define CONFIDENCE_THRESHOLD  0.70f

/* ラベル順序は Edge Impulse プロジェクトの classes 定義に合わせること
 * 現在の想定: 0=idle, 1=shake, 2=tap, 3=rotate  */

extern "C" int ai_init(void) {
    /* EON Compiler 使用時はアリーナ不要だが、念のため 0 クリア */
    for (int i = 0; i < EI_TENSOR_ARENA_SIZE; i++) tensor_arena[i] = 0;
    return 0;
}

extern "C" GestureID ai_classify(const float *samples, float *confidence_out) {
    /* Edge Impulse の signal_t を生成 */
    signal_t signal;
    int err = numpy::signal_from_buffer(
        samples,
        GC_WINDOW_SIZE * 3,  /* EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE と一致させること */
        &signal
    );
    if (err != 0) {
        if (confidence_out) *confidence_out = 0.0f;
        return GESTURE_IDLE;
    }

    ei_impulse_result_t result = {};
    EI_IMPULSE_ERROR ei_err = run_classifier(&signal, &result, false);
    if (ei_err != EI_IMPULSE_OK) {
        if (confidence_out) *confidence_out = 0.0f;
        return GESTURE_IDLE;
    }

    /* 最高スコアのクラスを探す */
    float best_score = 0.0f;
    int   best_idx   = 0;
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > best_score) {
            best_score = result.classification[i].value;
            best_idx   = i;
        }
    }

    if (confidence_out) *confidence_out = best_score;

    /* 閾値未満なら IDLE として扱う（誤検知抑制）*/
    return (best_score >= CONFIDENCE_THRESHOLD)
        ? (GestureID)best_idx
        : GESTURE_IDLE;
}
