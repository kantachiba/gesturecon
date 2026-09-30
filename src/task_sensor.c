/*
 * task_sensor.c
 * センサー収集タスク (優先度: PRI_SENSOR = 1 / 最高)
 *
 * 20ms (50Hz) 周期で LSM303AGR から 3 軸加速度を読み取り、
 * リングバッファに書き込む。GC_WINDOW_STRIDE サンプルごとに
 * ウィンドウを SensorMsg にコピーして AI タスクへ送信する。
 */
#include <gesture_con.h>
#include "ring_buffer.h"
#include "lsm303agr.h"

static RingBuf ring;

/* SensorMsg ダブルバッファ（AI 推論中に次のウィンドウを埋める）*/
static SensorMsg msg_pool[2];
static int pool_idx = 0;

void task_sensor(INT stacd, void *exinf) {
    (void)stacd; (void)exinf;

    rb_init(&ring);

    if (lsm303agr_init() != 0) {
        /* センサー初期化失敗 - 無限待機（デバッグ用）*/
        tk_slp_tsk(TMO_FEVR);
        return;
    }

    for (;;) {
        float ax, ay, az;
        if (lsm303agr_read(&ax, &ay, &az) == 0) {
            /* セマフォでリングバッファを保護（AI タスクの読み出しと排他）*/
            tk_wai_sem(sem_ring, 1, TMO_FEVR);
            rb_write(&ring, ax, ay, az);
            tk_sig_sem(sem_ring, 1);

            if (rb_ready(&ring)) {
                /* ウィンドウコピーして AI タスクへ送信 */
                SensorMsg *msg = &msg_pool[pool_idx];
                tk_wai_sem(sem_ring, 1, TMO_FEVR);
                rb_copy_window(&ring, msg->samples);
                tk_sig_sem(sem_ring, 1);

                tk_snd_mbx(mbx_sensor, (T_MSG *)msg);
                pool_idx ^= 1;   /* 次は逆バッファを使用 */
            }

            /* 活動フラグをセット（省電力タスクの無操作タイマーリセット）*/
            tk_set_flg(flg_system, FLG_ACTIVITY);
        }

        tk_dly_tsk(GC_SAMPLE_PERIOD_MS);
    }
}
