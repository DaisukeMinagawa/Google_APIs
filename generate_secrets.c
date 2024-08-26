#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

#define MAX_INPUT_LENGTH 256

void generate_secrets_file() {
    char client_id[MAX_INPUT_LENGTH];
    char client_secret[MAX_INPUT_LENGTH];
    char redirect_uri[MAX_INPUT_LENGTH];

    printf("クライアントIDを入力してください: ");
    fgets(client_id, MAX_INPUT_LENGTH, stdin);
    client_id[strcspn(client_id, "\n")] = '\0';

    printf("クライアントシークレットを入力してください: ");
    fgets(client_secret, MAX_INPUT_LENGTH, stdin);
    client_secret[strcspn(client_secret, "\n")] = '\0';

    printf("リダイレクトURIを入力してください: ");
    fgets(redirect_uri, MAX_INPUT_LENGTH, stdin);
    redirect_uri[strcspn(redirect_uri, "\n")] = '\0';

    struct json_object *secrets_json = json_object_new_object();
    json_object_object_add(secrets_json, "client_id", json_object_new_string(client_id));
    json_object_object_add(secrets_json, "client_secret", json_object_new_string(client_secret));
    json_object_object_add(secrets_json, "redirect_uri", json_object_new_string(redirect_uri));

    const char *secrets_str = json_object_to_json_string(secrets_json);

    FILE *file = fopen("secrets.json", "w");
    if (file == NULL) {
        fprintf(stderr, "エラー: secrets.json ファイルを開けません\n");
        json_object_put(secrets_json);
        return;
    }

    fprintf(file, "%s", secrets_str);
    fclose(file);

    json_object_put(secrets_json);

    printf("secrets.json ファイルが生成されました。\n");
}

int main() {
    generate_secrets_file();
    return 0;
}