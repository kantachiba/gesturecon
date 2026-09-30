# ジェスチャーコン 〜低遅延・非接触デバイス制御の実現〜

**TRONプログラミングコンテスト 応募作品**  
開発者: 千葉 幹太

---

## 概要

micro:bit v2 に内蔵された加速度センサーの 3 軸データを **Edge Impulse** で学習した TinyML モデルでリアルタイム識別し、外部機器（サーボモータ・LED）を**非接触**で操作するシステムです。  
**μT-Kernel 3.0** のマルチタスク機能により、AI 推論中もセンサーの取りこぼしゼロを実現しています。

## 認識ジェスチャー

| ジェスチャー | 動作 | 出力 |
|---|---|---|
| Shake（振る） | 左右に素早く 3 回振る | サーボ 0°（ドア解錠） |
| Tap（叩く） | 手首を 2 回コンコン叩く | サーボ 90°（中立） |
| Rotate（回す） | 手首を 180° ひねる | サーボ 180°（照明最大） |
| Idle（待機） | 日常動作 | 出力なし |

## システム構成

```
┌─────────────────────────────────────────────┐
│  micro:bit v2 (nRF52833 / μT-Kernel 3.0)    │
│                                             │
│  LSM303AGR ──→ [task_sensor  prio:1]        │
│               ──→ [task_ai_infer prio:5]    │
│                   ──→ [task_output  prio:8] │
│  GPIOTE INT ──→ [task_power  prio:10]       │
│                                             │
│  出力: LED マトリクス / サーボ PWM / GPIO    │
└─────────────────────────────────────────────┘
```

## ファイル構成

```
gesture_con/
├── include/gesture_con.h      共通定義・タスク優先度・メッセージ型
├── src/
│   ├── main.c                 usermain(): IPC生成・タスク起動
│   ├── ring_buffer.h/c        循環バッファ (200サンプル)
│   ├── lsm303agr.h/c          加速度センサードライバ (TWIM0直接制御)
│   ├── task_sensor.c          50Hzサンプリング → AI mailbox送信
│   ├── task_ai_infer.c        Edge Impulse推論 → 出力mailbox送信
│   ├── task_output.c          LEDマトリクス + PWMサーボ制御
│   ├── task_power.c           10秒無操作で省電力移行・割込み復帰
│   └── ai/
│       ├── ai_wrapper.h       C/C++ 境界インターフェース
│       └── ai_wrapper.cpp     Edge Impulse SDK ラッパー
└── makecode/
    └── data_collector.ts      訓練データ収集スクリプト (MakeCode)
```

## 開発環境

| 項目 | 内容 |
|---|---|
| ボード | micro:bit v2 (nRF52833) |
| RTOS | μT-Kernel 3.0 (IoTen PracKit / パーソナルメディア社) |
| IDE | パーソナルメディア社提供 μT-Kernel 3.0 開発環境 |
| AI | Edge Impulse (Spectral Analysis + Dense NN / INT8量子化) |
| 言語 | C (メイン) / C++ (Edge Impulse ラッパー) |
| OS | Windows 11 |

## ビルド手順

1. IoTen PracKit IDE を起動
2. `src/` と `include/` をプロジェクトに追加
3. Edge Impulse Studio でモデルを訓練し `C++ library` をダウンロード
4. 展開した SDK を `src/ai/` 以下に配置
5. コンパイルフラグ追加: `-fno-exceptions -fno-rtti -DEI_CLASSIFIER_USE_FULL_MALLOC=0`
6. ビルド → micro:bit に書き込み

## AI モデル訓練手順

1. `makecode/data_collector.ts` を micro:bit に書き込む
2. `edge-impulse-data-forwarder --frequency 50` で接続
3. 各クラス 100 サンプル × 2 秒を収集
4. Edge Impulse Studio: Spectral Analysis (FFT=16) → Dense(64→32→4) → INT8量子化
5. `C++ library` でエクスポート

## μT-Kernel 3.0 活用ポイント

- **タスク管理**: 4タスクを優先度 1/5/8/10 で独立動作
- **メールボックス**: タスク間データ受け渡し（コピー不要・ポインタ渡し）
- **セマフォ**: リングバッファの排他制御
- **イベントフラグ**: 省電力管理・活動検知
- **tk_set_pow**: 無操作 10 秒後の CPU サスペンド

## ライセンス

本プログラムは TRON プログラミングコンテスト応募のために開発しました。  
μT-Kernel 3.0 は T-License 2.2 に従います。  
Edge Impulse SDK は Apache License 2.0 に従います。
