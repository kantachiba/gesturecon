/*
 * lsm303agr.h
 * LSM303AGR 加速度センサードライバ (micro:bit v2 内蔵)
 * nRF52833 TWIM0 直接制御
 */
#ifndef LSM303AGR_H
#define LSM303AGR_H

/* I2C アドレス (SA0=HIGH on micro:bit v2) */
#define LSM303AGR_ACCEL_ADDR  0x19U

/*
 * lsm303agr_init - センサー初期化
 * 戻り値: 0=成功, -1=WHO_AM_I 不一致
 */
int lsm303agr_init(void);

/*
 * lsm303agr_read - 加速度データ読み出し
 * ax, ay, az: 出力先 [m/s^2]
 * 戻り値: 0=成功, -1=I2C エラー
 */
int lsm303agr_read(float *ax, float *ay, float *az);

#endif /* LSM303AGR_H */
