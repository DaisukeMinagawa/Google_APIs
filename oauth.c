#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include "config.h"
#include "oauth.h"

#define SECRETS_FILE "secrets.json"
#define BUFFER_SIZE 1024
#define MAX_INPUT_LENGTH 256

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

char* get_auth_code_from_local_server() {
    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/get_code");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.memory);
            return NULL;
        }

        curl_easy_cleanup(curl);
    }

    return chunk.memory;
}

int perform_oauth_flow(char **access_token) {
    char *client_id = get_config_value("client_id", SECRETS_FILE);
    char *client_secret = get_config_value("client_secret", SECRETS_FILE);
    char *redirect_uri = get_config_value("redirect_uri", SECRETS_FILE);

    if (client_id == NULL || client_secret == NULL || redirect_uri == NULL) {
        fprintf(stderr, "エラー: クライアント情報の取得に失敗しました\n");
        return 1;
    }

    char auth_url[BUFFER_SIZE];
    snprintf(auth_url, sizeof(auth_url),
             "https://accounts.google.com/o/oauth2/auth?client_id=%s&redirect_uri=%s&response_type=code&scope=https://www.googleapis.com/auth/calendar",
             client_id, redirect_uri);

    printf("次のURLにアクセスして認証を完了してください:\n%s\n", auth_url);
    printf("認証が完了したら、このプログラムに戻ってきてEnterキーを押してください。\n");
    getchar();

    char *auth_code = get_auth_code_from_local_server();
    if (auth_code == NULL) {
        fprintf(stderr, "エラー: 認証コードの取得に失敗しました\n");
        return 1;
    }

    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        char post_fields[BUFFER_SIZE];
        snprintf(post_fields, sizeof(post_fields),
                 "code=%s&client_id=%s&client_secret=%s&redirect_uri=%s&grant_type=authorization_code",
                 auth_code, client_id, client_secret, redirect_uri);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return 1;
        }

        struct json_object *parsed_json;
        struct json_object *access_token_obj;

        parsed_json = json_tokener_parse(chunk.memory);
        if (parsed_json == NULL) {
            fprintf(stderr, "エラー: JSONのパースに失敗しました\n");
            return 1;
        }

        if (!json_object_object_get_ex(parsed_json, "access_token", &access_token_obj)) {
            fprintf(stderr, "エラー: アクセストークンの取得に失敗しました\n");
            return 1;
        }

        *access_token = strdup(json_object_get_string(access_token_obj));

        json_object_put(parsed_json);
        curl_easy_cleanup(curl);
        free(chunk.memory);
    }

    curl_global_cleanup();
    free(client_id);
    free(client_secret);
    free(redirect_uri);
    free(auth_code);

    return 0;
}

// int main() {
//     char *access_token = NULL;
//     if (perform_oauth_flow(&access_token) == 0) {
//         printf("アクセストークンの取得に成功しました: %s\n", access_token);
//         free(access_token);
//     } else {
//         printf("OAuth認証に失敗しました。\n");
//     }
//     return 0;
// }