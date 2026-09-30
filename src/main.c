/*
 * main.c
 * ジェスチャーコン - アプリケーションエントリポイント
 *
 * μT-Kernel 3.0 の usermain() から呼ばれる。
 * タスク・メールボックス・セマフォ・イベントフラグを生成して起動する。
 */
#include <tk/tkernel.h>
#include <gesture_con.h>

/* ---- グローバル IPC オブジェクト ID ---- */
ID mbx_sensor  = 0;
ID mbx_result  = 0;
ID flg_system  = 0;
ID sem_ring    = 0;

/* ---- タスクスタック（TA_USERBUF 使用 / malloc 不要）---- */
static UB stack_sensor[STK_SENSOR];
static UB stack_ai    [STK_AI];
static UB stack_output[STK_OUTPUT];
static UB stack_power [STK_POWER];

static ER create_ipc(void) {
    ER err;

    /* メールボックス: センサー → AI */
    T_CMBX cmbx = { 0, TA_TFIFO | TA_MFIFO };
    err = tk_cre_mbx(&cmbx);
    if (err < 0) return err;
    mbx_sensor = (ID)err;

    /* メールボックス: AI → 出力 */
    err = tk_cre_mbx(&cmbx);
    if (err < 0) return err;
    mbx_result = (ID)err;

    /* イベントフラグ: システム全体 */
    T_CFLG cflg = { 0, TA_WMUL, 0 };
    err = tk_cre_flg(&cflg);
    if (err < 0) return err;
    flg_system = (ID)err;

    /* セマフォ: リングバッファ保護 (初期値=1, 最大=1) */
    T_CSEM csem = { 0, TA_TFIFO, 1, 1 };
    err = tk_cre_sem(&csem);
    if (err < 0) return err;
    sem_ring = (ID)err;

    return E_OK;
}

static ER create_tasks(void) {
    ER err;

    T_CTSK ctsk = {
        .exinf   = NULL,
        .tskatr  = TA_HLNG | TA_RNG0 | TA_USERBUF,
        .stksz   = 0,       /* bufptr から算出 */
        .bufptr  = NULL,
    };

    /* センサータスク */
    ctsk.task    = task_sensor;
    ctsk.itskpri = PRI_SENSOR;
    ctsk.stksz   = STK_SENSOR;
    ctsk.bufptr  = stack_sensor;
    err = tk_cre_tsk(&ctsk);
    if (err < 0) return err;
    tk_sta_tsk((ID)err, 0);

    /* AI 推論タスク */
    ctsk.task    = task_ai_infer;
    ctsk.itskpri = PRI_AI;
    ctsk.stksz   = STK_AI;
    ctsk.bufptr  = stack_ai;
    err = tk_cre_tsk(&ctsk);
    if (err < 0) return err;
    tk_sta_tsk((ID)err, 0);

    /* 出力制御タスク */
    ctsk.task    = task_output;
    ctsk.itskpri = PRI_OUTPUT;
    ctsk.stksz   = STK_OUTPUT;
    ctsk.bufptr  = stack_output;
    err = tk_cre_tsk(&ctsk);
    if (err < 0) return err;
    tk_sta_tsk((ID)err, 0);

    /* 省電力タスク */
    ctsk.task    = task_power;
    ctsk.itskpri = PRI_POWER;
    ctsk.stksz   = STK_POWER;
    ctsk.bufptr  = stack_power;
    err = tk_cre_tsk(&ctsk);
    if (err < 0) return err;
    tk_sta_tsk((ID)err, 0);

    return E_OK;
}

/*
 * usermain - μT-Kernel 初期タスクから呼ばれるエントリポイント
 * 全リソースを生成して各タスクを起動し、0 を返す。
 * 初期タスクは終了するが、生成されたタスクは動作し続ける。
 */
EXPORT INT usermain(void) {
    if (create_ipc()   != E_OK) goto fail;
    if (create_tasks() != E_OK) goto fail;
    return 0;

fail:
    /* 起動失敗 — 実機では LED エラー表示などを追加する */
    return -1;
}
