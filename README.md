# M5CoreS3_FaceRecognition
M5Stack CoreS3の内蔵カメラで顔検出及び顔認識するテストプログラム。  
フレームワークのexampleにあるCameraWebServerの顔検出＆顔認識の部分のコードを切り出して使っています。

フレームワークのexampleはここに保存されています（Windows10の場合）。  
C:\Users\\(ユーザ名)\\.platformio\packages\framework-arduinoespressif32\libraries\ESP32\examples


## 開発環境
VSCode + PlatformIO

## 依存ライブラリ
* M5Unified
* esp32-camera

## メモリ使用量
顔検出＆顔認識を追加することによるメモリ使用量の増分は次の通り。

- ビルド結果  
  - RAM：18KB増（グローバル変数などのstatic領域）
  - Flash：2.1MB増（プログラム、定数）
- 実行時ヒープ使用量
  - RAM：79KB増
  - PSRAM：230KB増
  
## 動作説明
電源を入れると画面にカメラ画像を表示しながら顔検出が走ります。顔を検出すると色のついた枠で顔が囲われます。  
顔検出しているときに画面タップすると、その顔にID（1～7）が付けられて登録されます。  
未登録の顔を検出すると赤色の枠が表示され、登録済みの顔を検出すると緑色の枠とIDが表示されます。

IDはFlashに保存されるため、電源OFFしても登録した顔の情報は消えません。  
IDを消去したいときは recognizer.clear_id(true) で消去できます。


