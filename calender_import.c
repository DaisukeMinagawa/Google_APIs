/**
 * Google Calendar イベントインポートツール
 * 
 * このプログラムは、OAuth 2.0認証を使用してGoogle Calendar APIにアクセスし、
 * ユーザーが指定したイベントをカレンダーにインポートします。
 * 
 * セキュリティ対策:
 * - 機密情報の安全な取り扱い
 * - 入力値の検証
 * - メモリの安全な管理
 * - エラー処理の強化
 * 
 * 注意: このコードを実際に使用する前に、セキュリティ専門家によるレビューを受けることをお勧めします。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curl/curl.h"
#include "json-c/json.h"
#include <time.h>
#include <locale.h>
#include <ctype.h>
#include <unistd.h>

#define CONFIG_FILE "config.json"
#define TOKEN_FILE "token.json"
#define BUFFER_SIZE 4096
#define MAX_INPUT_LENGTH 256
#define AUTH_URL "https://accounts.google.com/o/oauth2/v2/auth"
#define TOKEN_URL "https://oauth2.googleapis.com/token"
#define SCOPE "https://www.googleapis.com/auth/calendar.events"

// セキュリティ強化: バッファオーバーフロー対策のための安全な文字列操作マクロ
#define SAFE_STRCPY(dest, src, dest_size) \
    do { \
        strncpy(dest, src, (dest_size) - 1); \
        (dest)[(dest_size) - 1] = '\0'; \
    } while(0)

/**
 * メモリ構造体
 * CURLによって取得されたデータを格納するための構造体
 */
struct MemoryStruct {
    char *memory;  // 動的に割り当てられたメモリへのポインタ
    size_t size;   // 現在のメモリサイズ
};

/**
 * メモリコールバック関数
 * CURLがデータを受信するたびに呼び出される関数
 * 
 * @param contents 受信したデータ
 * @param size 各データ要素のサイズ
 * @param nmemb データ要素の数
 * @param userp ユーザーポインタ（MemoryStruct構造体へのポインタ）
 * @return 処理されたバイト数
 */
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        fprintf(stderr, "エラー: メモリ不足（reallocがNULLを返しました）\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

/**
 * ファイルの内容を読み取る関数
 * 
 * @param filename 読み取るファイルの名前
 * @return ファイルの内容を含む動的に割り当てられた文字列、失敗時はNULL
 */
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "エラー: ファイル %s を開けません\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = malloc(file_size + 1);
    if (content == NULL) {
        fprintf(stderr, "エラー: メモリ割り当てに失敗しました\n");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(content, 1, file_size, file);
    if (read_size != file_size) {
        fprintf(stderr, "エラー: ファイル全体の読み取りに失敗しました\n");
        free(content);
        fclose(file);
        return NULL;
    }

    content[file_size] = '\0';
    fclose(file);
    return content;
}

/**
 * 設定ファイルから特定の値を取得する関数
 * 
 * @param key 取得したい設定のキー
 * @return 設定値を含む動的に割り当てられた文字列、失敗時はNULL
 */
char* get_config_value(const char* key) {
    char* config_content = read_file(CONFIG_FILE);
    if (config_content == NULL) {
        return NULL;
    }

    struct json_object *parsed_json;
    struct json_object *value;

    parsed_json = json_tokener_parse(config_content);
    if (!parsed_json) {
        fprintf(stderr, "エラー: 設定ファイルの解析に失敗しました\n");
        free(config_content);
        return NULL;
    }

    if (json_object_object_get_ex(parsed_json, key, &value)) {
        char* result = strdup(json_object_get_string(value));
        json_object_put(parsed_json);
        free(config_content);
        return result;
    } else {
        fprintf(stderr, "エラー: キー '%s' が設定に見つかりません\n", key);
        json_object_put(parsed_json);
        free(config_content);
        return NULL;
    }
}

/**
 * 文字列をURL安全にエンコードする関数
 * 
 * @param input エンコードする文字列
 * @return エンコードされた文字列、失敗時はNULL
 */
char* url_encode(const char* input) {
    CURL *curl = curl_easy_init();
    if(curl) {
        char* output = curl_easy_escape(curl, input, 0);
        curl_easy_cleanup(curl);
        return output;
    }
    return NULL;
}

// ... [前のパートから続く]

/**
 * OAuth 2.0認証用のURLを生成する関数
 * 
 * @return 生成された認証URL、失敗時はNULL
 */
char* generate_auth_url() {
    char* client_id = get_config_value("client_id");
    char* redirect_uri = get_config_value("redirect_uri");
    
    if (!client_id || !redirect_uri) {
        fprintf(stderr, "エラー: client_idまたはredirect_uriの取得に失敗しました\n");
        free(client_id);
        free(redirect_uri);
        return NULL;
    }

    char* encoded_redirect_uri = url_encode(redirect_uri);
    char* encoded_scope = url_encode(SCOPE);
    
    if (!encoded_redirect_uri || !encoded_scope) {
        fprintf(stderr, "エラー: URLエンコードに失敗しました\n");
        free(client_id);
        free(redirect_uri);
        free(encoded_redirect_uri);
        free(encoded_scope);
        return NULL;
    }

    char* url = malloc(BUFFER_SIZE);
    if (!url) {
        fprintf(stderr, "エラー: メモリ割り当てに失敗しました\n");
        free(client_id);
        free(redirect_uri);
        free(encoded_redirect_uri);
        free(encoded_scope);
        return NULL;
    }

    // セキュリティ強化: バッファオーバーフロー対策としてsnprintfを使用
    int written = snprintf(url, BUFFER_SIZE, "%s?client_id=%s&redirect_uri=%s&response_type=code&scope=%s",
             AUTH_URL, client_id, encoded_redirect_uri, encoded_scope);

    if (written < 0 || written >= BUFFER_SIZE) {
        fprintf(stderr, "エラー: 認証URLの生成に失敗しました\n");
        free(url);
        url = NULL;
    }
    
    free(client_id);
    free(redirect_uri);
    curl_free(encoded_redirect_uri);
    curl_free(encoded_scope);
    
    return url;
}

/**
 * 認証コードをアクセストークンと交換する関数
 * 
 * @param code 認証コード
 * @return トークンレスポンスを含む文字列、失敗時はNULL
 */
char* exchange_code_for_token(const char* code) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if(curl) {
        char* client_id = get_config_value("client_id");
        char* client_secret = get_config_value("client_secret");
        char* redirect_uri = get_config_value("redirect_uri");

        if (!client_id || !client_secret || !redirect_uri) {
            fprintf(stderr, "エラー: 必要な設定値の取得に失敗しました\n");
            free(client_id);
            free(client_secret);
            free(redirect_uri);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return NULL;
        }

        char* post_fields = malloc(BUFFER_SIZE);
        if (!post_fields) {
            fprintf(stderr, "エラー: メモリ割り当てに失敗しました\n");
            free(client_id);
            free(client_secret);
            free(redirect_uri);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return NULL;
        }

        // セキュリティ強化: バッファオーバーフロー対策としてsnprintfを使用
        int written = snprintf(post_fields, BUFFER_SIZE,
                 "code=%s&client_id=%s&client_secret=%s&redirect_uri=%s&grant_type=authorization_code",
                 code, client_id, client_secret, redirect_uri);

        if (written < 0 || written >= BUFFER_SIZE) {
            fprintf(stderr, "エラー: POSTフィールドの生成に失敗しました\n");
            free(client_id);
            free(client_secret);
            free(redirect_uri);
            free(post_fields);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        // セキュリティ強化: SSL証明書の検証を有効化
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.memory);
            chunk.memory = NULL;
        }

        curl_easy_cleanup(curl);
        free(client_id);
        free(client_secret);
        free(redirect_uri);
        free(post_fields);
    }

    curl_global_cleanup();

    return chunk.memory;
}

/**
 * トークンをファイルに保存する関数
 * 
 * @param token_response 保存するトークンレスポンス
 * @return 成功時は0、失敗時は-1
 */
int save_token(const char* token_response) {
    // セキュリティ強化: ファイルのパーミッションを制限
    mode_t old_mask = umask(0077);
    FILE* file = fopen(TOKEN_FILE, "w");
    umask(old_mask);

    if (file == NULL) {
        fprintf(stderr, "エラー: トークンファイルを書き込み用に開けません\n");
        return -1;
    }

    fputs(token_response, file);
    fclose(file);
    return 0;
}

/**
 * リフレッシュトークンを使用して新しいアクセストークンを取得する関数
 * 
 * @return 新しいトークンレスポンス、失敗時はNULL
 */
char* refresh_token() {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if(curl) {
        char* client_id = get_config_value("client_id");
        char* client_secret = get_config_value("client_secret");
        char* refresh_token = get_config_value("refresh_token");

        if (!client_id || !client_secret || !refresh_token) {
            fprintf(stderr, "エラー: 必要な設定値の取得に失敗しました\n");
            free(client_id);
            free(client_secret);
            free(refresh_token);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return NULL;
        }

        char* post_fields = malloc(BUFFER_SIZE);
        if (!post_fields) {
            fprintf(stderr, "エラー: メモリ割り当てに失敗しました\n");
            free(client_id);
            free(client_secret);
            free(refresh_token);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return NULL;
        }

        // セキュリティ強化: バッファオーバーフロー対策としてsnprintfを使用
        int written = snprintf(post_fields, BUFFER_SIZE,
                 "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token",
                 client_id, client_secret, refresh_token);

        if (written < 0 || written >= BUFFER_SIZE) {
            fprintf(stderr, "エラー: POSTフィールドの生成に失敗しました\n");
            free(client_id);
            free(client_secret);
            free(refresh_token);
            free(post_fields);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return NULL;
        }

        curl_easy_setopt(curl, CURLOPT_URL, TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        // セキュリティ強化: SSL証明書の検証を有効化
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.memory);
            chunk.memory = NULL;
        }

        curl_easy_cleanup(curl);
        free(client_id);
        free(client_secret);
        free(refresh_token);
        free(post_fields);
    }

    curl_global_cleanup();

    return chunk.memory;
}

// ... [前のパートから続く]

/**
 * 有効なアクセストークンを取得する関数
 * トークンが期限切れの場合は自動的に更新を試みる
 * 
 * @return 有効なアクセストークン、失敗時はNULL
 */
char* get_valid_access_token() {
    char* token_content = read_file(TOKEN_FILE);
    if (token_content == NULL) {
        return NULL;
    }

    struct json_object *parsed_json;
    struct json_object *access_token, *expires_in, *created_at;

    parsed_json = json_tokener_parse(token_content);
    if (!parsed_json) {
        fprintf(stderr, "エラー: トークンファイルの解析に失敗しました\n");
        free(token_content);
        return NULL;
    }

    json_object_object_get_ex(parsed_json, "access_token", &access_token);
    json_object_object_get_ex(parsed_json, "expires_in", &expires_in);
    json_object_object_get_ex(parsed_json, "created_at", &created_at);

    time_t now = time(NULL);
    time_t token_expiry = json_object_get_int64(created_at) + json_object_get_int64(expires_in);

    if (now >= token_expiry) {
        printf("トークンの有効期限が切れています。更新中...\n");
        char* new_token_response = refresh_token();
        if (new_token_response) {
            if (save_token(new_token_response) != 0) {
                fprintf(stderr, "エラー: 新しいトークンの保存に失敗しました\n");
                free(new_token_response);
                json_object_put(parsed_json);
                free(token_content);
                return NULL;
            }
            free(new_token_response);
            json_object_put(parsed_json);
            free(token_content);
            return get_valid_access_token();  // 新しいトークンを取得するための再帰呼び出し
        } else {
            fprintf(stderr, "エラー: トークンの更新に失敗しました\n");
            json_object_put(parsed_json);
            free(token_content);
            return NULL;
        }
    } else {
        char* result = strdup(json_object_get_string(access_token));
        json_object_put(parsed_json);
        free(token_content);
        return result;
    }
}

/**
 * Google Calendarにイベントをインポートする関数
 * 
 * @param calendar_id インポート先のカレンダーID
 * @param event_data インポートするイベントのJSONデータ
 * @return 成功時は0、失敗時は-1
 */
int import_event(const char* calendar_id, const char* event_data) {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if(curl) {
        char url[BUFFER_SIZE];
        // セキュリティ強化: バッファオーバーフロー対策としてsnprintfを使用
        int written = snprintf(url, sizeof(url), "https://www.googleapis.com/calendar/v3/calendars/%s/events/import", calendar_id);
        if (written < 0 || written >= sizeof(url)) {
            fprintf(stderr, "エラー: URLの生成に失敗しました\n");
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return -1;
        }

        char* access_token = get_valid_access_token();
        if (access_token == NULL) {
            fprintf(stderr, "エラー: 有効なアクセストークンの取得に失敗しました\n");
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return -1;
        }

        char auth_header[BUFFER_SIZE];
        written = snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
        if (written < 0 || written >= sizeof(auth_header)) {
            fprintf(stderr, "エラー: 認証ヘッダーの生成に失敗しました\n");
            free(access_token);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            free(chunk.memory);
            return -1;
        }

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, auth_header);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, event_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        // セキュリティ強化: SSL証明書の検証を有効化
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            fprintf(stderr, "エラー: curl_easy_perform() が失敗しました: %s\n", curl_easy_strerror(res));
        } else {
            printf("イベントが正常にインポートされました。レスポンス: %s\n", chunk.memory);
        }

        free(access_token);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    free(chunk.memory);
    curl_global_cleanup();

    return (res == CURLE_OK) ? 0 : -1;
}

/**
 * 認証手順を表示する関数
 * 
 * @param auth_url 認証URL
 */
void print_auth_instructions(const char* auth_url) {
    printf("以下の手順に従って認証を行ってください：\n");
    printf("1. 以下のURLをブラウザで開いてください：\n%s\n", auth_url);
    printf("2. Googleアカウントでログインしてください（まだログインしていない場合）。\n");
    printf("3. アプリケーションがカレンダーにアクセスすることを許可してください。\n");
    printf("4. 許可後、ブラウザに表示される認証コードをコピーしてください。\n");
}

/**
 * ユーザーから認証コードを安全に取得する関数
 * 
 * @return 認証コード、失敗時はNULL
 */
char* get_authorization_code() {
    char* code = malloc(MAX_INPUT_LENGTH);
    if (!code) {
        fprintf(stderr, "エラー: メモリ割り当てに失敗しました\n");
        return NULL;
    }

    printf("認証コードを入力してください: ");
    if (fgets(code, MAX_INPUT_LENGTH, stdin) == NULL) {
        fprintf(stderr, "エラー: 認証コードの読み取りに失敗しました\n");
        free(code);
        return NULL;
    }

    // 改行文字を削除
    code[strcspn(code, "\n")] = 0;

    // セキュリティ強化: 入力値の検証
    for (int i = 0; code[i]; i++) {
        if (!isalnum(code[i]) && code[i] != '-' && code[i] != '_' && code[i] != '.') {
            fprintf(stderr, "エラー: 無効な文字が含まれています\n");
            free(code);
            return NULL;
        }
    }

    return code;
}

/**
 * OAuth認証フローを実行する関数
 * 
 * @return 成功時は0、失敗時は-1
 */
int perform_oauth_flow() {
    char* auth_url = generate_auth_url();
    if (!auth_url) {
        fprintf(stderr, "エラー: 認証URLの生成に失敗しました\n");
        return -1;
    }

    print_auth_instructions(auth_url);
    free(auth_url);

    char* auth_code = get_authorization_code();
    if (!auth_code) {
        return -1;
    }

    char* token_response = exchange_code_for_token(auth_code);
    free(auth_code);

    if (token_response) {
        if (save_token(token_response) != 0) {
            fprintf(stderr, "エラー: トークンの保存に失敗しました\n");
            free(token_response);
            return -1;
        }
        free(token_response);
        printf("認証が成功しました。\n");
        return 0;
    } else {
        fprintf(stderr, "エラー: トークンの取得に失敗しました\n");
        return -1;
    }
}

// ... [メイン関数は次のセッションに続きます]// ... [前のパートから続く]

/**
 * ユーザーからイベントの詳細を安全に取得する関数
 * 
 * @param event_summary イベントのタイトルを格納する文字列
 * @param event_start イベントの開始日時を格納する文字列
 * @param event_end イベントの終了日時を格納する文字列
 */
void get_event_details(char* event_summary, char* event_start, char* event_end) {
    printf("イベントの詳細を入力してください：\n");

    printf("イベントのタイトル: ");
    if (fgets(event_summary, MAX_INPUT_LENGTH, stdin) == NULL) {
        fprintf(stderr, "エラー: イベントタイトルの読み取りに失敗しました\n");
        exit(1);
    }
    event_summary[strcspn(event_summary, "\n")] = 0;

    printf("開始日時 (YYYY-MM-DDTHH:MM:SS): ");
    if (fgets(event_start, MAX_INPUT_LENGTH, stdin) == NULL) {
        fprintf(stderr, "エラー: 開始日時の読み取りに失敗しました\n");
        exit(1);
    }
    event_start[strcspn(event_start, "\n")] = 0;

    printf("終了日時 (YYYY-MM-DDTHH:MM:SS): ");
    if (fgets(event_end, MAX_INPUT_LENGTH, stdin) == NULL) {
        fprintf(stderr, "エラー: 終了日時の読み取りに失敗しました\n");
        exit(1);
    }
    event_end[strcspn(event_end, "\n")] = 0;

    // セキュリティ強化: 入力値の検証
    if (!validate_datetime(event_start) || !validate_datetime(event_end)) {
        fprintf(stderr, "エラー: 無効な日時形式です\n");
        exit(1);
    }
}

/**
 * 日時形式を検証する関数
 * 
 * @param datetime 検証する日時文字列
 * @return 有効な形式の場合は1、そうでない場合は0
 */
int validate_datetime(const char* datetime) {
    int year, month, day, hour, minute, second;
    return (sscanf(datetime, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6);
}

/**
 * メイン関数
 * プログラムの全体的な流れを制御する
 */
int main(void) {
    setlocale(LC_ALL, "");  // 日本語出力のために必要

    printf("Google Calendar イベントインポートツール\n\n");

    // トークンファイルが存在しない場合、OAuth フローを実行
    FILE* token_file = fopen(TOKEN_FILE, "r");
    if (token_file == NULL) {
        printf("初回認証が必要です。\n");
        if (perform_oauth_flow() != 0) {
            fprintf(stderr, "エラー: 認証に失敗しました\n");
            return 1;
        }
    } else {
        fclose(token_file);
    }

    char* calendar_id = get_config_value("calendar_id");
    if (calendar_id == NULL) {
        fprintf(stderr, "エラー: カレンダーIDの取得に失敗しました\n");
        return 1;
    }

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
        return 1;
    }

    int result = import_event(calendar_id, event_data);

    if (result == 0) {
        printf("イベントが正常にインポートされました。\n");
    } else {
        fprintf(stderr, "エラー: イベントのインポートに失敗しました。\n");
    }

    free(calendar_id);
    return result;
}

/**
 * プログラムの使用方法を表示する関数
 */
void print_usage() {
    printf("使用方法:\n");
    printf("1. config.jsonファイルを作成し、以下の情報を記入してください：\n");
    printf("   {\n");
    printf("     \"client_id\": \"YOUR_CLIENT_ID\",\n");
    printf("     \"client_secret\": \"YOUR_CLIENT_SECRET\",\n");
    printf("     \"redirect_uri\": \"urn:ietf:wg:oauth:2.0:oob\",\n");
    printf("     \"calendar_id\": \"primary\"\n");
    printf("   }\n\n");
    printf("2. プログラムを実行します。\n");
    printf("3. 初回実行時は、表示されるURLにアクセスして認証を行ってください。\n");
    printf("4. 認証後、イベントの詳細を入力してください。\n");
}

// エラーハンドリング用のマクロ
#define HANDLE_ERROR(condition, message) \
    do { \
        if (condition) { \
            fprintf(stderr, "エラー: %s\n", message); \
            exit(1); \
        } \
    } while(0)

// メモリ解放用のマクロ
#define SAFE_FREE(ptr) \
    do { \
        if (ptr) { \
            free(ptr); \
            ptr = NULL; \
        } \
    } while(0)