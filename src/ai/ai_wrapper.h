/*
 * ai_wrapper.h
 * Edge Impulse C++ ライブラリを C から呼び出すためのインターフェース
 *
 * Edge Impulse プロジェクトから C++ library をダウンロードし、
 * プロジェクトの ai/ ディレクトリに展開してから使用する。
 *
 * デプロイ手順:
 *   1. Edge Impulse Studio → Deployment → C++ Library → Build
 *   2. ZIP を解凍して edge-impulse-sdk/ と model-parameters/ を
 *      このディレクトリ（ai/）に配置
 *   3. コンパイルフラグ:
 *        -fno-exceptions -fno-rtti
 *        -DEI_CLASSIFIER_USE_FULL_MALLOC=0
 *        -std=gnu++14
 *      ※ GCC 9 を使用すること（micro:bit v2 の codal 既知問題）
 */
#ifndef AI_WRAPPER_H
#define AI_WRAPPER_H

#include <gesture_con.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TFLite Micro 静的アリーナサイズ（RAM 20KB 確保）*/
#define EI_TENSOR_ARENA_SIZE  (20 * 1024)

/*
 * ai_init - Edge Impulse ランタイム初期化
 * 戻り値: 0=成功, -1=失敗
 */
int ai_init(void);

/*
 * ai_classify - ジェスチャー推論
 * samples: [GC_WINDOW_SIZE * 3] 配列 [ax0,ay0,az0, ax1,ay1,az1, ...]
 *          単位: m/s^2
 * confidence_out: 最高スコアの確信度（NULL 可）
 * 戻り値: GestureID（スコア < 0.7 の場合は GESTURE_IDLE）
 */
GestureID ai_classify(const float *samples, float *confidence_out);

#ifdef __cplusplus
}
#endif

#endif /* AI_WRAPPER_H */
