/*
 * lsm303agr.c
 * LSM303AGR 加速度センサードライバ
 *
 * micro:bit v2 の内部 I2C バス:
 *   TWIM0, SCL=P0.08, SDA=P0.16
 *   LSM303AGR accel addr = 0x19 (SA0=HIGH)
 */
#include "lsm303agr.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* nRF52833 TWIM0 レジスタ (オフセット指定で直接アクセス)              */
/* ------------------------------------------------------------------ */
#define TWIM0_BASE  0x40003000UL
#define TWIM0(off)  (*(volatile uint32_t*)(TWIM0_BASE + (off)))

#define TASKS_STARTRX   0x000U
#define TASKS_STARTTX   0x008U
#define TASKS_STOP      0x014U
#define EVENTS_STOPPED  0x104U
#define EVENTS_ERROR    0x124U
#define SHORTS          0x200U
#define ENABLE          0x500U
#define PSEL_SCL        0x508U
#define PSEL_SDA        0x50CU
#define FREQUENCY       0x524U
#define RXD_PTR         0x534U
#define RXD_MAXCNT      0x538U
#define TXD_PTR         0x544U
#define TXD_MAXCNT      0x548U
#define ADDRESS         0x588U

#define TWIM_ENABLE_Enabled       6U
#define TWIM_FREQ_100K            0x01980000UL
/* SHORTS ビット: LASTTX→STARTRX(0x01), LASTRX→STOP(0x02), LASTTX→STOP(0x80) */
#define SHORT_LASTTX_STARTRX      0x01U
#define SHORT_LASTRX_STOP         0x02U
#define SHORT_LASTTX_STOP         0x80U

/* ---- GPIO P0 PIN_CNF レジスタ ---- */
#define GPIO0_BASE      0x50000000UL
#define GPIO0_PIN_CNF(n) (*(volatile uint32_t*)(GPIO0_BASE + 0x700U + (n)*4U))

/* PIN_CNF = S0D1 drive (open-drain high) + pullup + input connected */
#define PIN_CNF_I2C     0x00000600U   /* DRIVE=S0D1(6<<8), no pull, input=connect, dir=input */

#define SCL_PIN  8U
#define SDA_PIN 16U

/* ---- LSM303AGR レジスタ ---- */
#define REG_WHO_AM_I   0x0FU
#define REG_CTRL1_A    0x20U
#define REG_CTRL4_A    0x23U
#define REG_OUT_X_L_A  0x28U

/* CTRL_REG1_A: ODR=50Hz, Normal mode, XYZ全軸有効 */
#define CTRL1_50HZ_NORMAL  0x47U
/* CTRL_REG4_A: BDU=1, ±2g, High-Resolution */
#define CTRL4_HR_2G_BDU    0x88U

/* HR モード ±2g: 1 LSB = 1 mg = 0.001 g */
#define ACCEL_SENSITIVITY_G  0.001f
#define G_TO_MS2             9.80665f

/* ------------------------------------------------------------------ */
/* TWIM ドライバ                                                        */
/* ------------------------------------------------------------------ */

static void twim_init(void) {
    GPIO0_PIN_CNF(SCL_PIN) = PIN_CNF_I2C;
    GPIO0_PIN_CNF(SDA_PIN) = PIN_CNF_I2C;

    TWIM0(ENABLE)    = 0;
    TWIM0(PSEL_SCL)  = SCL_PIN;
    TWIM0(PSEL_SDA)  = SDA_PIN;
    TWIM0(FREQUENCY) = TWIM_FREQ_100K;
    TWIM0(ENABLE)    = TWIM_ENABLE_Enabled;
}

/* レジスタ書き込み: [reg, val] を送信して STOP */
static int twim_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    TWIM0(ADDRESS)        = addr;
    TWIM0(TXD_PTR)        = (uint32_t)(uintptr_t)buf;
    TWIM0(TXD_MAXCNT)     = 2;
    TWIM0(EVENTS_STOPPED) = 0;
    TWIM0(EVENTS_ERROR)   = 0;
    TWIM0(SHORTS)         = SHORT_LASTTX_STOP;
    TWIM0(TASKS_STARTTX)  = 1;
    while (!TWIM0(EVENTS_STOPPED) && !TWIM0(EVENTS_ERROR));
    return TWIM0(EVENTS_ERROR) ? -1 : 0;
}

/* バースト読み出し: [reg] 送信 → 連続受信 → STOP (repeated-start) */
static int twim_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, int len) {
    TWIM0(ADDRESS)        = addr;
    TWIM0(TXD_PTR)        = (uint32_t)(uintptr_t)&reg;
    TWIM0(TXD_MAXCNT)     = 1;
    TWIM0(RXD_PTR)        = (uint32_t)(uintptr_t)buf;
    TWIM0(RXD_MAXCNT)     = (uint32_t)len;
    TWIM0(EVENTS_STOPPED) = 0;
    TWIM0(EVENTS_ERROR)   = 0;
    /* TX完了→RX開始(repeated-start), RX完了→STOP */
    TWIM0(SHORTS)        = SHORT_LASTTX_STARTRX | SHORT_LASTRX_STOP;
    TWIM0(TASKS_STARTTX) = 1;
    while (!TWIM0(EVENTS_STOPPED) && !TWIM0(EVENTS_ERROR));
    return TWIM0(EVENTS_ERROR) ? -1 : 0;
}

/* ------------------------------------------------------------------ */
/* LSM303AGR 公開 API                                                   */
/* ------------------------------------------------------------------ */

int lsm303agr_init(void) {
    twim_init();

    uint8_t who = 0;
    if (twim_read_regs(LSM303AGR_ACCEL_ADDR, REG_WHO_AM_I, &who, 1) != 0)
        return -1;
    if (who != 0x33)
        return -1;   /* センサーが見つからない */

    if (twim_write_reg(LSM303AGR_ACCEL_ADDR, REG_CTRL1_A, CTRL1_50HZ_NORMAL) != 0)
        return -1;
    if (twim_write_reg(LSM303AGR_ACCEL_ADDR, REG_CTRL4_A, CTRL4_HR_2G_BDU) != 0)
        return -1;

    return 0;
}

int lsm303agr_read(float *ax, float *ay, float *az) {
    /* OUT_X_L_A の MSB=1 でアドレス自動インクリメント */
    uint8_t raw[6];
    if (twim_read_regs(LSM303AGR_ACCEL_ADDR, REG_OUT_X_L_A | 0x80U, raw, 6) != 0)
        return -1;

    /* 16bit 左詰め → 右シフト4で 12-bit 符号付き値を取得 */
    int16_t rx = (int16_t)(((uint16_t)raw[1] << 8) | raw[0]) >> 4;
    int16_t ry = (int16_t)(((uint16_t)raw[3] << 8) | raw[2]) >> 4;
    int16_t rz = (int16_t)(((uint16_t)raw[5] << 8) | raw[4]) >> 4;

    *ax = (float)rx * ACCEL_SENSITIVITY_G * G_TO_MS2;
    *ay = (float)ry * ACCEL_SENSITIVITY_G * G_TO_MS2;
    *az = (float)rz * ACCEL_SENSITIVITY_G * G_TO_MS2;
    return 0;
}
