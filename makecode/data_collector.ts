/**
 * data_collector.ts
 * Edge Impulse データ収集スクリプト (MakeCode / micro:bit v2)
 *
 * 【動作概要】
 *   常時 50Hz で ax,ay,az をシリアル出力し続ける。
 *   録画の「開始/停止」は Edge Impulse Studio 側で行う。
 *   ボタン B で現在収集中のジェスチャークラス名を LED に表示（確認用）。
 *
 * 【セットアップ手順】
 *   1. MakeCode (https://makecode.microbit.org) で
 *      「New Project → JavaScript」に貼り付けてコンパイル → micro:bit に書き込む
 *   2. PC で: npm install -g edge-impulse-cli
 *   3. micro:bit を USB 接続した状態で:
 *        edge-impulse-data-forwarder --frequency 50
 *      → ブラウザが開くので Edge Impulse Studio のプロジェクトにログイン
 *      → 軸名を聞かれたら "ax, ay, az" と入力
 *   4. Studio の [Data acquisition] タブでラベルを設定して収集
 */

const LABELS = ["idle", "shake", "tap", "rotate"];
let labelIdx = 0;

// ---- ボタン B: 現在のジェスチャー名を LED に表示（確認用）----
input.onButtonPressed(Button.B, function () {
  labelIdx = (labelIdx + 1) % LABELS.length;
  basic.showString(LABELS[labelIdx][0].toUpperCase());
});

// ---- シリアルポート明示初期化（Windows で認識されない場合に必要）----
serial.redirectToUSB()
serial.setBaudRate(BaudRate.BaudRate115200)

// ---- 起動時に "I" (idle) を表示 ----
basic.showString("I");

// ---- メインループ: 50 Hz 常時ストリーミング ----
basic.forever(function () {
  const ax = input.acceleration(Dimension.X);
  const ay = input.acceleration(Dimension.Y);
  const az = input.acceleration(Dimension.Z);
  // edge-impulse-data-forwarder が期待するフォーマット: "val1,val2,val3\n"
  serial.writeLine(`${ax},${ay},${az}`);
  basic.pause(20); // 50 Hz
});
