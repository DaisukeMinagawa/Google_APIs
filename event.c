#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "event.h"

#define BUFFER_SIZE 1024

// イベントをインポートする関数
int import_event(const char* calendar_id, const char* event_data, const char* access_token) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if(curl) {
        char url[BUFFER_SIZE];
        snprintf(url, sizeof(url), "https://www.googleapis.com/calendar/v3/calendars/%s/events", calendar_id);

        headers = curl_slist_append(headers, "Content-Type: application/json");
        char auth_header[BUFFER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
        headers = curl_slist_append(headers, auth_header);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, event_data);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return 1;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
    }

    curl_global_cleanup();
    return 0;
}