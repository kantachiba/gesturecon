/*
 * task_ai_infer.c
 * AI 推論タスク (優先度: PRI_AI = 5)
 *
 * センサータスクから SensorMsg を受信し、Edge Impulse モデルで
 * ジェスチャーを識別して ResultMsg を出力タスクへ送信する。
 * 推論中もセンサータスクは最高優先度で動作し続けるため、
 * サンプルの取りこぼしが発生しない。
 */
#include <gesture_con.h>
#include "ai/ai_wrapper.h"

static ResultMsg result_msg;

void task_ai_infer(INT stacd, void *exinf) {
    (void)stacd; (void)exinf;

    if (ai_init() != 0) {
        tk_slp_tsk(TMO_FEVR);
        return;
    }

    for (;;) {
        /* センサータスクからウィンドウデータを受信（無限待機）*/
        T_MSG *raw_msg;
        tk_rcv_mbx(mbx_sensor, &raw_msg, TMO_FEVR);
        SensorMsg *smsg = (SensorMsg *)raw_msg;

        /* Edge Impulse 推論（~80ms @ 64MHz Cortex-M4F）*/
        float confidence = 0.0f;
        GestureID gesture = ai_classify(smsg->samples, &confidence);

        /* IDLE でなければ出力タスクへ通知 */
        if (gesture != GESTURE_IDLE) {
            result_msg.gesture    = gesture;
            result_msg.confidence = confidence;
            tk_snd_mbx(mbx_result, (T_MSG *)&result_msg);
        }
    }
}
