#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "config.h"

#define SECRETS_FILE "secrets.json"
#define CONFIG_FILE "config.json"

// ファイルを読み込む関数
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "エラー: ファイル %s を開けません\n", filename);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)malloc(length + 1);
    if (content == NULL) {
        fprintf(stderr, "エラー: メモリの割り当てに失敗しました\n");
        fclose(file);
        return NULL;
    }

    fread(content, 1, length, file);
    content[length] = '\0';

    fclose(file);
    return content;
}

// 設定値を取得する関数
char* get_config_value(const char* key, const char* filename) {
    char* config_content = read_file(filename);
    if (config_content == NULL) {
        fprintf(stderr, "エラー: %s の読み取りに失敗しました\n", filename);
        return NULL;
    }

    struct json_object *parsed_json;
    struct json_object *value_obj;

    parsed_json = json_tokener_parse(config_content);
    if (parsed_json == NULL) {
        fprintf(stderr, "エラー: JSONのパースに失敗しました\n");
        free(config_content);
        return NULL;
    }

    if (!json_object_object_get_ex(parsed_json, key, &value_obj)) {
        fprintf(stderr, "エラー: キー %s が見つかりません\n", key);
        json_object_put(parsed_json); // メモリ解放
        free(config_content);
        return NULL;
    }

    const char* value = json_object_get_string(value_obj);
    char* result = strdup(value); // 値をコピー

    json_object_put(parsed_json); // メモリ解放
    free(config_content);

    return result;
}