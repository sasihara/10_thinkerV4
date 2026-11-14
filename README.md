# 06_thinkerV3_PyTorch
## 概要
["AlphaZero 深層学習・強化学習・探索 人工知能プログラミング実践入門"(株式会社ボーンデジタル)](https://www.amazon.co.jp/AlphaZero-%E6%B7%B1%E5%B1%A4%E5%AD%A6%E7%BF%92%E3%83%BB%E5%BC%B7%E5%8C%96%E5%AD%A6%E7%BF%92%E3%83%BB%E6%8E%A2%E7%B4%A2-%E4%BA%BA%E5%B7%A5%E7%9F%A5%E8%83%BD%E3%83%97%E3%83%AD%E3%82%B0%E3%83%A9%E3%83%9F%E3%83%B3%E3%82%B0%E5%AE%9F%E8%B7%B5%E5%85%A5%E9%96%80-%E5%B8%83%E7%95%99%E5%B7%9D-%E8%8B%B1%E4%B8%80/dp/4862464505?__mk_ja_JP=%E3%82%AB%E3%82%BF%E3%82%AB%E3%83%8A&crid=23HZL4ACT6NSF&dib=eyJ2IjoiMSJ9.9y8dkCsxh6_b-E-08N8xpeDeVLz_V2TNs9JjN531QtZbtkYoejpXi39tF0i-FZlYv08LWfnnQl35z7IQWIGHrxp8oko4wszBsUQu3oEgCpGFy-q0FTrPxGndzIm7Q9lIRPLJV-NVNfud-pIoj4U75WIBuRMAcGofA00xKI9J4w_wC1hXeVf8oFVj-m2yCbtOSOHYSsZsw4sfOL6-5sJtCkoCD0p8kI23l4qKcjuBpN_lvm15k56BxH5rVhW-lmmI65a5fzHxOrBov1B2ai9zEGS_4D2L0HX5_p7zAtHvJlqf0-wO9QY9Ys6ZgDYgBy4a.kumXFWo33VfDXqxrPTODo8r8vfv143HWsXDT6t4x_-4&dib_tag=se&keywords=AlphaZero+%E6%B7%B1%E5%B1%A4%E5%AD%A6%E7%BF%92%E3%83%BB%E5%BC%B7%E5%8C%96%E5%AD%A6%E7%BF%92%E3%83%BB%E6%8E%A2%E7%B4%A2+%E4%BA%BA%E5%B7%A5%E7%9F%A5%E8%83%BD%E3%83%97%E3%83%AD%E3%82%B0%E3%83%A9%E3%83%9F%E3%83%B3%E3%82%B0%E5%AE%9F%E8%B7%B5%E5%85%A5%E9%96%80&qid=1741977020&sprefix=alphazero+%E6%B7%B1%E5%B1%A4%E5%AD%A6%E7%BF%92+%E5%BC%B7%E5%8C%96%E5%AD%A6%E7%BF%92+%E6%8E%A2%E7%B4%A2+%E4%BA%BA%E5%B7%A5%E7%9F%A5%E8%83%BD%E3%83%97%E3%83%AD%E3%82%B0%E3%83%A9%E3%83%9F%E3%83%B3%E3%82%B0%E5%AE%9F%E8%B7%B5%E5%85%A5%E9%96%80%2Caps%2C175&sr=8-1&linkCode=ll1&tag=sasihara-22&linkId=790dfcb949f1ba93d358633f6dfb1682&language=ja_JP&ref_=as_li_ss_tl)を参考に、C++言語で実装したDeep Learningベースの思考ルーチンです。
TensorFlowベースの03_thinkerV3をPyTorchベースに変更したものです｡

min-max版に対して勝率86%を達成した学習済みモデルも同梱しています｡

## 制限事項
週末の趣味程度で開発しているものなので、完成度は期待しないで下さい。
## 推奨環境
Intel Core i5 2.4GHz程度のCPUであれば十分遊べますが、GPUがあると思考時間がより短くなります。
## 開発環境
Microsoft Visual Studio Community 2026での動作を確認しております。それ以外の環境については未確認ですが、基本的なAPIしか使用してないため、他のバージョンでも動作する可能性は高いと思います。
## コンパイル・実行
- 本ソースのコンパイルには、レポジトリ"01_othello"に含まれるソースも必要ですので、レポジトリ"01_othello"もダウンロードしておいて下さい。
- プロジェクトファイルはthinkerV3_PyTorch.slnxです。Visual Studioでオープンしコンパイルすると、実行バイナリがx64\Releaseフォルダの下に作成されますので、それらをダブルクリックすることで実行できます。
- コンパイルおよび本思考ルーチンの実行には、LibTorchで提供されるファイル一式が必要となります｡[PyTorchのページ](https://pytorch.org/)からダウンロードして下さい｡
  - ページ中程のInstall PyTorchの所で､Your OSを"Windows"､Packageを"LibTorch"､Languageを"C++/Java"､Compute Platformを適切に選んでダウンロード･解凍してください｡コンパイルする場合は解凍したフォルダ一式を06_thinkerV3_PyTorchフォルダと同じフォルダに格納する必要があります｡
  - 26/3/1現在のStable版(2.10.0)でコンパイル､CPU上で動作できることを確認しております｡
  - バイナリを実行するには､環境変数pathにlibtorch\libフォルダを追加した後､マシンを再起動する必要があります｡
  - (GPU上で動かす方法については現在調査中)
- thnkerV3.exeを実行すると、デフォルトではUDPポート番号60001でメッセージ待ち受け状態に入ります。"01_othello"プロジェクトに格納されるothello.exeを実行し、
ボード上をクリックすることで表示されるゲームの初期設定画面において、"Computer(External)"にチェックを入れた後、黒もしくは白のHost Nameに"localhost"を、Portに"60001"をセットすることで、本思考ルーチンを用いてプレーすることができます。
- 起動時のオプションは以下の通り｡

|オプション|意味|
|-|-|
|-T[temperature]|Temperature(>= 0.0)|
|-t[temperature]|序盤(最初の10手)のみTemperature(>= 0.0)を適用する｡|
|-b|モンテカルロ法で探索する際に､横優先で探索する｡|
|-C|CPUを使って推論する｡|
|-G(gpuid)|GPUを使って推論する｡-Gの後にGPU番号を指定してGPUを特定するのも可能｡使用できない場合はエラーコードを出力して終了する｡|
|-g(gpuid)|GPUを使って推論する｡-gの後にGPU番号を指定してGPUを特定するのも可能｡使用できない場合はCPU使用に切り替える｡|
|-M|Min-Maxアルゴリズムを使用する(シングルプロセッサ)｡|
|-M(numThreads)|numThreadsで指定した数のスレッドを使って､Min-Maxアルゴリズムを使用する｡シングル版に比べ探索の深さが1ステップ深い｡|

## 開発履歴
### 2026/3/20
マルチプロセッサを使用したMin-Maxアルゴリズムでの動作も可能とした｡
### 2026/3/1
thinkerV3をPyTorchへ移植｡
### 2025/11/9(V3.20)
強さに直接影響を与えるモンテカルロ法処理部分の価値計算におけるバグを修正｡
### 2025/5/10
終盤はDeep Learningベースではなく、完全読みを行うように修正。
### 2025/3/9
初期バージョン。
