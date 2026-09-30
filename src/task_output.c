/*
 * task_output.c
 * 出力制御タスク (優先度: PRI_OUTPUT = 8)
 *
 * AI タスクからジェスチャー結果を受信し、
 * LED マトリクスとサーボモータを制御する。
 *
 * micro:bit v2 LED マトリクス (5×5):
 *   ROW1-5: P0.21, P0.22, P0.15, P0.24, P0.19
 *   COL1-5: P0.28, P0.11, P0.31, P0.05, P0.30
 *   ROW=HIGH, COL=LOW で点灯
 *
 * サーボ: エッジコネクタ P0 (= nRF52833 P0.02) に PWM 出力
 *   PWM 周期: 20ms, パルス幅: 1ms=0°, 1.5ms=90°, 2ms=180°
 */
#include <gesture_con.h>
#include <stdint.h>

/* ---- nRF52833 GPIO P0 ---- */
#define GPIO0_BASE       0x50000000UL
#define GPIO0(off)       (*(volatile uint32_t*)(GPIO0_BASE + (off)))
#define GPIO0_OUTSET     0x508U
#define GPIO0_OUTCLR     0x50CU
#define GPIO0_DIRSET     0x518U
#define GPIO0_PIN_CNF(n) (*(volatile uint32_t*)(GPIO0_BASE + 0x700U + (n)*4U))

/* LED マトリクス GPIO ピン番号 */
#define LED_ROW1  21U
#define LED_ROW2  22U
#define LED_ROW3  15U
#define LED_ROW4  24U
#define LED_ROW5  19U
#define LED_COL1  28U
#define LED_COL2  11U
#define LED_COL3  31U
#define LED_COL4   5U
#define LED_COL5  30U

/* ---- nRF52833 PWM0 (サーボ) ---- */
#define PWM0_BASE  0x4001C000UL
#define PWM0(off)  (*(volatile uint32_t*)(PWM0_BASE + (off)))
#define PWM_TASKS_SEQSTART0  0x008U
#define PWM_EVENTS_SEQEND0   0x110U
#define PWM_ENABLE           0x500U
#define PWM_MODE             0x504U
#define PWM_COUNTERTOP       0x508U
#define PWM_PRESCALER        0x50CU
#define PWM_PSEL_OUT0        0x560U
#define PWM_SEQ0_PTR         0x520U
#define PWM_SEQ0_CNT         0x524U
#define PWM_SEQ0_REFRESH     0x528U
#define PWM_SEQ0_ENDDELAY    0x52CU
#define PWM_LOOP             0x518U
#define PWM_DECODER          0x510U

#define SERVO_PIN  2U   /* エッジコネクタ P0 = nRF52833 P0.02 */

/* PWM 周期 20ms: COUNTERTOP=20000 (1MHz クロック, PRESCALER=4 → 1MHz) */
#define PWM_COUNTERTOP_VAL  20000U
/* サーボパルス幅 (カウント値 = us, 1MHz クロック想定) */
#define SERVO_0DEG   1000U   /* 1.0 ms */
#define SERVO_90DEG  1500U   /* 1.5 ms */
#define SERVO_180DEG 2000U   /* 2.0 ms */

static uint16_t pwm_seq[4];   /* PWM シーケンスバッファ */

/* ---- LED マトリクスパターン ----
 * [row_pin, col_pin] のペアリスト (0 終端) で1パターン定義
 * RTOS タスクのため、フル点灯パターンは単純化して1行ずつ点灯 */
typedef struct { uint32_t row; uint32_t col; } LedPixel;

static const LedPixel pat_shake[] = {
    /* ↑↓矢印（シェイク）: 中央縦ライン */
    {LED_ROW1, LED_COL3}, {LED_ROW2, LED_COL3},
    {LED_ROW3, LED_COL3}, {LED_ROW4, LED_COL3},
    {LED_ROW5, LED_COL3}, {0, 0}
};
static const LedPixel pat_tap[] = {
    /* ✓チェック（タップ）*/
    {LED_ROW3, LED_COL1}, {LED_ROW4, LED_COL2},
    {LED_ROW5, LED_COL3}, {LED_ROW4, LED_COL4},
    {LED_ROW3, LED_COL5}, {0, 0}
};
static const LedPixel pat_rotate[] = {
    /* ○円（ローテート）*/
    {LED_ROW1, LED_COL2}, {LED_ROW1, LED_COL3}, {LED_ROW1, LED_COL4},
    {LED_ROW2, LED_COL5}, {LED_ROW3, LED_COL5},
    {LED_ROW4, LED_COL5}, {LED_ROW5, LED_COL4},
    {LED_ROW5, LED_COL3}, {LED_ROW5, LED_COL2},
    {LED_ROW4, LED_COL1}, {LED_ROW3, LED_COL1},
    {LED_ROW2, LED_COL1}, {0, 0}
};

/* ------------------------------------------------------------------ */
/* GPIO 初期化                                                          */
/* ------------------------------------------------------------------ */
static void gpio_init(void) {
    /* 全 ROW/COL を OUTPUT に設定 */
    const uint32_t rows = (1U<<LED_ROW1)|(1U<<LED_ROW2)|(1U<<LED_ROW3)|
                          (1U<<LED_ROW4)|(1U<<LED_ROW5);
    const uint32_t cols = (1U<<LED_COL1)|(1U<<LED_COL2)|(1U<<LED_COL3)|
                          (1U<<LED_COL4)|(1U<<LED_COL5);
    GPIO0(GPIO0_DIRSET) = rows | cols;
    GPIO0(GPIO0_OUTCLR) = rows | cols;   /* 全消灯 */
}

/* ------------------------------------------------------------------ */
/* LED 表示                                                             */
/* ------------------------------------------------------------------ */
static void led_show(const LedPixel *pat, int duration_ms) {
    /* パターンを duration_ms の間、簡易フリッカー点灯 */
    int loops = duration_ms / 10;
    for (int l = 0; l < loops; l++) {
        for (int i = 0; pat[i].row != 0; i++) {
            GPIO0(GPIO0_OUTCLR) = 0xFFFFFFFFU;               /* 全消灯 */
            GPIO0(GPIO0_OUTSET) = (1U << pat[i].row);         /* ROW HIGH */
            GPIO0(GPIO0_OUTCLR) = (1U << pat[i].col);         /* COL LOW  */
            /* 短時間点灯（簡易遅延: CPUサイクル）*/
            for (volatile int d = 0; d < 500; d++);
            GPIO0(GPIO0_OUTCLR) = (1U << pat[i].row);         /* ROW LOW（消灯）*/
        }
        tk_dly_tsk(1);   /* 1ms 待機（他タスクに CPU を譲る）*/
    }
    /* パターン終了後に全消灯 */
    GPIO0(GPIO0_OUTCLR) = (1U<<LED_ROW1)|(1U<<LED_ROW2)|(1U<<LED_ROW3)|
                          (1U<<LED_ROW4)|(1U<<LED_ROW5)|
                          (1U<<LED_COL1)|(1U<<LED_COL2)|(1U<<LED_COL3)|
                          (1U<<LED_COL4)|(1U<<LED_COL5);
}

/* ------------------------------------------------------------------ */
/* サーボ PWM 初期化・設定                                              */
/* ------------------------------------------------------------------ */
static void servo_init(void) {
    GPIO0_PIN_CNF(SERVO_PIN) = 0x00000001U;   /* DIR=Output */
    GPIO0(GPIO0_DIRSET) = (1U << SERVO_PIN);

    PWM0(PWM_ENABLE)      = 0;
    PWM0(PWM_PSEL_OUT0)   = SERVO_PIN;
    PWM0(PWM_MODE)        = 0;              /* Up counter */
    PWM0(PWM_COUNTERTOP)  = PWM_COUNTERTOP_VAL;
    PWM0(PWM_PRESCALER)   = 4;             /* 16MHz / 2^4 = 1MHz */
    PWM0(PWM_DECODER)     = 0;             /* Common, Individual */
    PWM0(PWM_LOOP)        = 0;             /* Disabled (continuous) */
    pwm_seq[0] = (uint16_t)SERVO_90DEG;   /* 初期位置 90° */
    pwm_seq[1] = pwm_seq[2] = pwm_seq[3] = (uint16_t)SERVO_90DEG;
    PWM0(PWM_SEQ0_PTR)    = (uint32_t)(uintptr_t)pwm_seq;
    PWM0(PWM_SEQ0_CNT)    = 4;
    PWM0(PWM_SEQ0_REFRESH)   = 0;
    PWM0(PWM_SEQ0_ENDDELAY)  = 0;
    PWM0(PWM_ENABLE)         = 1;
    PWM0(PWM_TASKS_SEQSTART0) = 1;
}

static void servo_set(uint16_t pulse_us) {
    pwm_seq[0] = pulse_us;
    PWM0(PWM_TASKS_SEQSTART0) = 1;
}

/* ------------------------------------------------------------------ */
/* タスク本体                                                           */
/* ------------------------------------------------------------------ */
void task_output(INT stacd, void *exinf) {
    (void)stacd; (void)exinf;

    gpio_init();
    servo_init();

    for (;;) {
        /* AI タスクからジェスチャー結果を受信（無限待機）*/
        T_MSG *raw_msg;
        tk_rcv_mbx(mbx_result, &raw_msg, TMO_FEVR);
        ResultMsg *rmsg = (ResultMsg *)raw_msg;

        switch (rmsg->gesture) {
        case GESTURE_SHAKE:
            led_show(pat_shake, 500);
            servo_set(SERVO_0DEG);    /* ドア解錠: 0° */
            break;

        case GESTURE_TAP:
            led_show(pat_tap, 300);
            servo_set(SERVO_90DEG);   /* 中立位置 */
            break;

        case GESTURE_ROTATE:
            led_show(pat_rotate, 600);
            servo_set(SERVO_180DEG);  /* 照明最大: 180° */
            break;

        default:
            break;
        }
    }
}
