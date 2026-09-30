/*
 * task_power.c
 * 省電力管理タスク (優先度: PRI_POWER = 10 / 最低)
 *
 * INACTIVITY_TIMEOUT_MS の間、FLG_ACTIVITY が立たなければ
 * μT-Kernel の省電力 API で CPU を suspend させる。
 * 加速度センサーのしきい値割り込み（INT1/P0.25）で復帰する。
 *
 * INT1 ピン: nRF52833 P0.25 (micro:bit v2 内部接続)
 */
#include <gesture_con.h>
#include <stdint.h>

/* ---- nRF52833 GPIOTE (GPIO Tasks and Events) ---- */
#define GPIOTE_BASE         0x40006000UL
#define GPIOTE(off)         (*(volatile uint32_t*)(GPIOTE_BASE + (off)))
#define GPIOTE_INTENSET     0x304U
#define GPIOTE_CONFIG(n)    (*(volatile uint32_t*)(GPIOTE_BASE + 0x510U + (n)*4U))
#define GPIOTE_EVENTS_IN(n) (*(volatile uint32_t*)(GPIOTE_BASE + 0x100U + (n)*4U))

/* ---- NVIC (割り込みコントローラ) ---- */
#define NVIC_ISER0          (*(volatile uint32_t*)0xE000E100U)
#define GPIOTE_IRQn         6U   /* nRF52833 GPIOTE 割り込み番号 */

/* LSM303AGR INT1 ピン */
#define ACCEL_INT1_PIN  25U   /* P0.25 */

static volatile int wakeup_pending = 0;

/* ---- LSM303AGR アクティビティ割り込み設定（I2C で直接設定）---- */
/* lsm303agr.c の twim_write_reg を使いたいが、他 .c ファイル経由のため
 * ここでは簡易版を再実装する。将来は lsm303agr.h で公開する。     */
#define TWIM0_BASE2  0x40003000UL
#define TWIM0R(off)  (*(volatile uint32_t*)(TWIM0_BASE2 + (off)))

static void accel_enable_activity_int(void) {
    /* CTRL_REG3_A (0x22): INT1 に AOI1/ZHIE/YHIE/XHIE 割り込みルート */
    uint8_t buf[2];
    buf[0] = 0x22U;   /* REG_CTRL3_A */
    buf[1] = 0x40U;   /* INT1_AOI1 (アクティビティ割り込み有効) */
    TWIM0R(0x588) = 0x19U;                    /* ADDRESS = accel */
    TWIM0R(0x544) = (uint32_t)(uintptr_t)buf; /* TXD.PTR */
    TWIM0R(0x548) = 2;                        /* TXD.MAXCNT */
    TWIM0R(0x104) = 0; TWIM0R(0x124) = 0;    /* clear events */
    TWIM0R(0x200) = 0x80U;                    /* SHORTS: LASTTX_STOP */
    TWIM0R(0x008) = 1;                        /* TASKS_STARTTX */
    while (!TWIM0R(0x104) && !TWIM0R(0x124));

    /* INT1_THS_A (0x32): しきい値 = 250mg */
    buf[0] = 0x32U; buf[1] = 0x10U;
    TWIM0R(0x104) = 0; TWIM0R(0x124) = 0;
    TWIM0R(0x008) = 1;
    while (!TWIM0R(0x104) && !TWIM0R(0x124));

    /* INT1_DURATION_A (0x33): 持続時間 = 1 サンプル */
    buf[0] = 0x33U; buf[1] = 0x01U;
    TWIM0R(0x104) = 0; TWIM0R(0x124) = 0;
    TWIM0R(0x008) = 1;
    while (!TWIM0R(0x104) && !TWIM0R(0x124));
}

/* ---- GPIOTE 割り込みハンドラ（μT-Kernel 割り込みハンドラ形式）---- */
static void accel_int_handler(UINT dintno) {
    (void)dintno;
    if (GPIOTE_EVENTS_IN(0)) {
        GPIOTE_EVENTS_IN(0) = 0;
        wakeup_pending = 1;
        /* 省電力タスクを起こすフラグをセット（ISR 内は tk_set_flg 可）*/
        tk_set_flg(flg_system, FLG_WAKE_REQ);
    }
}

static void gpiote_init(void) {
    /* GPIOTE channel 0: ACCEL_INT1_PIN 立ち上がりエッジ検出 */
    GPIOTE_CONFIG(0) = (0x01U)             /* MODE=Event(1) */
                     | (ACCEL_INT1_PIN << 8)
                     | (0U << 13)           /* PORT=0 */
                     | (0x01U << 16);       /* POLARITY=LoToHi(1) */
    GPIOTE_EVENTS_IN(0) = 0;
    GPIOTE_INTENSET = 0x01U;               /* channel 0 割り込み有効 */

    /* μT-Kernel 割り込みハンドラ登録 */
    T_DINT dint = { TA_HLNG, accel_int_handler };
    tk_def_int(GPIOTE_IRQn, &dint);

    NVIC_ISER0 |= (1U << GPIOTE_IRQn);    /* NVIC 有効化 */
    tk_ena_int(GPIOTE_IRQn);
}

/* ------------------------------------------------------------------ */
/* タスク本体                                                           */
/* ------------------------------------------------------------------ */
void task_power(INT stacd, void *exinf) {
    (void)stacd; (void)exinf;

    accel_enable_activity_int();
    gpiote_init();

    for (;;) {
        UINT flgptn = 0;
        /* INACTIVITY_TIMEOUT_MS 待機、その間に FLG_ACTIVITY が来たらリセット */
        ER err = tk_wai_flg(flg_system, FLG_ACTIVITY | FLG_WAKE_REQ,
                            TWF_ORW | TWF_BITCLR, &flgptn,
                            (TMO)INACTIVITY_TIMEOUT_MS);

        if (err == E_TMOUT) {
            /* タイムアウト → 省電力移行 */
            tk_set_flg(flg_system, FLG_SLEEP_REQ);
            tk_set_pow(TPW_DOSUSPEND);

            /* tk_set_pow から戻ってきた = 割り込み復帰 */
            wakeup_pending = 0;
        }
        /* FLG_WAKE_REQ または FLG_ACTIVITY でループ続行（タイマーリセット）*/
    }
}
