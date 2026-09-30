/*
 * gesture_con.h
 * ジェスチャーコン - 共通定義
 */
#ifndef GESTURE_CON_H
#define GESTURE_CON_H

#include <tk/tkernel.h>

/* ---- サンプリングパラメータ ---- */
#define GC_SAMPLE_PERIOD_MS  20    /* 50 Hz */
#define GC_WINDOW_SIZE       100   /* 2秒 × 50Hz = 100サンプル */
#define GC_WINDOW_STRIDE     50    /* 推論トリガー間隔（50%オーバーラップ） */
#define GC_RING_SIZE         200   /* リングバッファ容量 */

/* ---- ジェスチャークラス ---- */
typedef enum {
    GESTURE_IDLE   = 0,
    GESTURE_SHAKE  = 1,
    GESTURE_TAP    = 2,
    GESTURE_ROTATE = 3,
    GESTURE_COUNT  = 4
} GestureID;

/* ---- タスク優先度 ---- */
#define PRI_SENSOR    1   /* 最高 - サンプル取りこぼし防止 */
#define PRI_AI        5   /* 中   - 推論処理（~100ms）*/
#define PRI_OUTPUT    8   /* 低   - 表示・アクチュエータ */
#define PRI_POWER    10   /* 最低 - アイドル管理 */

/* ---- タスクスタックサイズ (bytes) ---- */
#define STK_SENSOR   1024
#define STK_AI       4096   /* TFLite Micro 用に大きめ確保 */
#define STK_OUTPUT   1024
#define STK_POWER     512

/* ---- 無操作タイムアウト ---- */
#define INACTIVITY_TIMEOUT_MS  10000U  /* 10秒で省電力移行 */

/* ---- メールボックスメッセージ構造体 ----
 * μT-Kernel メールボックスプロトコル: T_MSG を先頭メンバに置く */

typedef struct {
    T_MSG   msgque;                        /* 必須ヘッダ */
    float   samples[GC_WINDOW_SIZE * 3];  /* [n][ax,ay,az] フラット配列 */
} SensorMsg;

typedef struct {
    T_MSG     msgque;      /* 必須ヘッダ */
    GestureID gesture;
    float     confidence;
} ResultMsg;

/* ---- グローバル IPC オブジェクト ID ---- */
extern ID mbx_sensor;   /* SensorMsg: センサー → AI */
extern ID mbx_result;   /* ResultMsg: AI → 出力 */
extern ID flg_system;   /* システム全体イベントフラグ */
extern ID sem_ring;     /* リングバッファアクセス保護 */

/* ---- イベントフラグビット ---- */
#define FLG_ACTIVITY    0x01U   /* センサーが活動を検出 */
#define FLG_SLEEP_REQ   0x02U   /* 省電力移行要求 */
#define FLG_WAKE_REQ    0x04U   /* 割り込みによる復帰 */

/* ---- タスクエントリ関数宣言 ---- */
void task_sensor(INT stacd, void *exinf);
void task_ai_infer(INT stacd, void *exinf);
void task_output(INT stacd, void *exinf);
void task_power(INT stacd, void *exinf);

#endif /* GESTURE_CON_H */
