#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include "config.h"
#include "oauth.h"
#include "event.h"

#define CONFIG_FILE "config.json"
#define TOKEN_FILE "token.json"
#define MAX_INPUT_LENGTH 256
#define BUFFER_SIZE 1024

void get_event_details(char* summary, char* start, char* end) {
    printf("イベントの概要を入力してください: ");
    fgets(summary, MAX_INPUT_LENGTH, stdin);
    summary[strcspn(summary, "\n")] = '\0';

    printf("イベントの開始日時を入力してください (例: 2023-01-01T10:00:00): ");
    fgets(start, MAX_INPUT_LENGTH, stdin);
    start[strcspn(start, "\n")] = '\0';

    printf("イベントの終了日時を入力してください (例: 2023-01-01T11:00:00): ");
    fgets(end, MAX_INPUT_LENGTH, stdin);
    end[strcspn(end, "\n")] = '\0';
}

int main(void) {
    setlocale(LC_ALL, "");  // 日本語出力のために必要

    printf("Google Calendar イベントインポートツール\n\n");

    // OAuth 2.0フローを実行してアクセストークンを取得
    char *access_token = NULL;
    if (perform_oauth_flow(&access_token) != 0) {
        fprintf(stderr, "エラー: OAuth 2.0フローに失敗しました\n");
        return 1;
    }

    printf("アクセストークンの取得に成功しました: %s\n", access_token);

    char* calendar_id = get_config_value("calendar_id", CONFIG_FILE);
    if (calendar_id == NULL) {
        fprintf(stderr, "エラー: カレンダーIDの取得に失敗しました\n");
        free(access_token);
        return 1;
    }

    printf("カレンダーID: %s\n", calendar_id); // カレンダーIDを出力

    char event_summary[MAX_INPUT_LENGTH];
    char event_start[MAX_INPUT_LENGTH];
    char event_end[MAX_INPUT_LENGTH];

    get_event_details(event_summary, event_start, event_end);

    char event_data[BUFFER_SIZE];
    int written = snprintf(event_data, sizeof(event_data),
             "{\"summary\":\"%s\",\"start\":{\"dateTime\":\"%s\"},\"end\":{\"dateTime\":\"%s\"}}",
             event_summary, event_start, event_end);
    
    if (written < 0 || written >= sizeof(event_data)) {
        fprintf(stderr, "エラー: イベントデータの生成に失敗しました\n");
        free(calendar_id);
        free(access_token);
        return 1;
    }

    int result = import_event(calendar_id, event_data, access_token);

    if (result == 0) {
        printf("イベントが正常にインポートされました。\n");
    } else {
        fprintf(stderr, "エラー: イベントのインポートに失敗しました。\n");
    }

    free(calendar_id);
    free(access_token);

    return result;
}