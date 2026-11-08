#include <stdio.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include "history.hpp"
#include "PTHHandler.hpp"

// Global
extern Logging logging;

int History::init()
{
	for (size_t i = 0; i < historyDataList.size(); i++) {
		historyDataList[i].historyData.clear();
	}

	historyDataList.clear();

	return 0;
}

int History::add(GameId _gameId, DISKCOLORS _diskcolor, DISKCOLORS* _board, std::vector<Score> scores)
{
	logging.logout("History::add()開始. isGameIdValid = %d, 保存済みgameId.pid = %d, 指定gameId.pid = %d.",
		isGameIdValid,
		gameId.pid,
		_gameId.pid
		);

	// HistoryDataの準備
	HistoryData historyData;

	// diskcolorのセット
	historyData.diskcolor = _diskcolor;

	// board内容のセット
#ifdef _WIN32
	memmove_s(&historyData.board, sizeof(historyData.board), _board, sizeof(historyData.board));
#else
	memmove(&historyData.board, _board, sizeof(historyData.board));
#endif

	// probabilityのセット
	// ベクタ型scoresに格納されるprobability値を_probability配列へ格納する
	double probability[DN_OUTPUT_SIZE];
	memset(probability, 0, sizeof(probability));

	for (size_t i = 0; i < scores.size(); i++) {
		int idx = scores[i].x * 8 + scores[i].y;

		if (idx < DN_OUTPUT_SIZE) {
			probability[idx] = scores[i].probability;
		}
	}

#ifdef _WIN32
	memmove_s(&historyData.probability, sizeof(historyData.probability), probability, sizeof(probability));
#else
	memmove(&historyData.probability, probability, sizeof(probability));
#endif

	// value値の初期化
	historyData.value = 0.0;

	// 該当のgameIdのヒストリが既に存在するか？
	bool isHistoryListExist = false;
	size_t index = 0;

	for (index = 0; index < historyDataList.size(); index++) {
		// 既に指定されたgameIdのヒストリデータリストが存在する場合
		if (historyDataList[index].gameId == _gameId && historyDataList[index].diskcolor == _diskcolor) {
			isHistoryListExist = true;
			break;
		}
	}

	// ヒストリデータを保存する
	// ヒストリデータリストが見つからなかった場合は新しく作ってそこに格納
	if (isHistoryListExist == false) {
		// すでに規定数以上のトランザクション数のヒストリデータが保存されていれば先頭の学習データを消す
		if (historyDataList.size() >= MAX_NUM_TRANSAC_HIST) {
			// historyDataListの先頭を削除
			historyDataList.begin()->historyData.clear();
			historyDataList.erase(historyDataList.begin());
		}
			
		// 新しいトランザクションのヒストリデータを保存する
		HistoryDataList newHistoryDataList;

		newHistoryDataList.gameId = _gameId;
		newHistoryDataList.diskcolor = _diskcolor;
		newHistoryDataList.historyData.push_back(historyData);

		historyDataList.push_back(newHistoryDataList);
	}
	else {
		historyDataList[index].historyData.push_back(historyData);
	}

	logging.logout("ヒストリデータを保存しました.");

	return 0;
}

int History::finish(GameId _gameId, DISKCOLORS _diskcolor, float _value)
{
	logging.logout("History::setValue() start.");

	for (size_t i = 0; i < historyDataList.size(); i++) {
		if (historyDataList[i].gameId == _gameId && historyDataList[i].diskcolor == _diskcolor) {
			logging.logout("value = %dをセットします.", _value);

			// 現在保存されているヒストリ情報にvalue値をセットする
			for (size_t j = 0; j < historyDataList[i].historyData.size(); j++) {
				if (historyDataList[i].historyData[j].diskcolor == _diskcolor) {
					historyDataList[i].historyData[j].value = _value;
				}
			}

			// ヒストリ情報をファイルに出力する
			logging.logout("学習データをファイルに出力します.");

			int ret;
			ret = outputFile(_gameId, _diskcolor);

			if (ret < 0) {
				return -1;
			}

			// データを削除する
			historyDataList[i].historyData.clear();
			historyDataList.erase(historyDataList.begin() + i);

			logging.logout("History::setValue() finish.");
			return 0;
		}
	}

	// 指定されたgameIdのヒストリデータが保存されてない場合
	logging.logout("[WARNING] gameId = %d の学習データが無いので値のセットおよびファイル出力は行いません.", _gameId);
	logging.logout("History::setValue() finish.");
	return -2;
}

int History::outputFile(GameId _gameId, DISKCOLORS _diskcolor)
{
	logging.logout("History::outputFile() start.");

	for (size_t i = 0; i < historyDataList.size(); i++) {
		if (historyDataList[i].gameId == _gameId && historyDataList[i].diskcolor == _diskcolor) {
			// ヒストリデータの有無を確認する
			if (historyDataList[i].historyData.size() <= 0) {
				return -1;
			}

			// 学習データ格納用フォルダを作成する
#ifdef _WIN32
			if (_mkdir("history") != 0) {
#else
			if (mkdir("history", 0777) != 0) {
#endif
				if(errno != EEXIST) return -2;
			}

			// ファイル名を決定する
			// ファイル名は"年月日時分秒石の色.hst"
			time_t currentTime, ret;
			struct tm localTime;
			char fileName[256];

			ret = time(&currentTime);
			if (ret < 0) return -3;

			int err;
#ifdef _WIN32
			err = localtime_s(&localTime, &currentTime);
#else
			localtime_r(&currentTime, &localTime);
			err = 0;
#endif

			if (err) {
				return -4;
			}

#ifdef _WIN32
			sprintf_s(fileName, "history\\%04d%02d%02d%02d%02d%02d%c_%d.bhs",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				_diskcolor == DISKCOLORS::COLOR_BLACK ? 'B' : 'W',
				_gameId.pid
			);
#else
			snprintf(fileName, sizeof(fileName), "history/%04d%02d%02d%02d%02d%02d%c_%d.bhs",
				localTime.tm_year + 1900,
				localTime.tm_mon + 1,
				localTime.tm_mday,
				localTime.tm_hour,
				localTime.tm_min,
				localTime.tm_sec,
				_diskcolor == DISKCOLORS::COLOR_BLACK ? 'B' : 'W',
				_gameId.pid
			);
#endif

			// ファイルをオープンする
			FILE* f;
#ifdef _WIN32
			if (fopen_s(&f, fileName, "wb") != 0) {
#else
			f = fopen(fileName, "wb");
			if (f == nullptr) {
#endif
				return -5;
			}

			// ファイルを書き込む
			uint16_t  formatVersion = FORMAT_VERSION;

			// フォーマットバージョン
			fwrite(&formatVersion, sizeof(formatVersion), 1, f);

			for (size_t j = 0; j < historyDataList[i].historyData.size(); j++) {
				if (historyDataList[i].historyData[j].diskcolor == _diskcolor) {
					// 下で出力されるポリシー値・value値がどちらの石の色に対する値なのかを出力
					fwrite(&historyDataList[i].historyData[j].diskcolor, sizeof(historyDataList[i].historyData[j].diskcolor), 1, f);

					// 盤面の書き込み
					fwrite(historyDataList[i].historyData[j].board, sizeof(historyDataList[i].historyData[j].board), 1, f);

					// ポリシー値の書き込み
					fwrite(historyDataList[i].historyData[j].probability, sizeof(historyDataList[i].historyData[j].probability), 1, f);

					// value値の書き込み
					fwrite(&historyDataList[i].historyData[j].value, sizeof(historyDataList[i].historyData[j].value), 1, f);
				}
			}

			// ファイルをクローズする
			fclose(f);
			return 0;
		}
	}

	// 指定されたgameIdのヒストリデータが保存されてない場合
	logging.logout("[WARNING] gameId = %d の学習データが無いのでファイル出力は行いません.", _gameId);
	logging.logout("History::outputFile() finish.");
	return -6;
}
