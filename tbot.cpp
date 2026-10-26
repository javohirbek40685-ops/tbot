#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include "json.hpp"

using json = nlohmann::json;

const char* env_bot = std::getenv("BOT_TOKEN");
const char* env_gemini = std::getenv("GEMINI_API_KEY");

const std::string BOT_TOKEN = env_bot ? env_bot : "";
const std::string GEMINI_API_KEY = env_gemini ? env_gemini : "";

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string sendHttpRequest(const std::string& url, const std::string& postData) {
    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

std::string getAIResponse(const std::string& prompt) {
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + GEMINI_API_KEY;
    
    json req;
    // Tizim ko'rsatmasi to'g'ri JSON array formatida
    req["system_instruction"]["parts"] = json::array({ 
        { {"text", "Sening isming Voice. Sen Javohirbekning shaxsiy sun'iy intellekt yordamchisi va virtual ovozisan. Foydalanuvchilarga Javohirbek nomidan xushmuomala, aniq va o'zbek tilida javob ber. O'zingni hech qachon OpenAI yoki boshqa model deb atama, har doim Voice ekanligingni bildir."} } 
    });

    json msg_content;
    msg_content["role"] = "user";
    msg_content["parts"] = json::array({ { {"text", prompt} } });
    req["contents"] = json::array({ msg_content });

    std::string resp = sendHttpRequest(url, req.dump());
    try {
        auto j = json::parse(resp);
        if (j.contains("candidates") && !j["candidates"].empty()) {
            return j["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        } else if (j.contains("error")) {
            std::cout << "API Error: " << j["error"]["message"].get<std::string>() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "JSON Parse error: " << e.what() << std::endl;
    }
    
    return "Hozircha javob bera olmayman.";
}

void sendTelegramMessage(long long chat_id, const std::string& text, const std::string& business_conn_id) {
    std::string url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage";
    json req;
    req["chat_id"] = chat_id;
    req["text"] = text;
    if (!business_conn_id.empty()) {
        req["business_connection_id"] = business_conn_id;
    }
    sendHttpRequest(url, req.dump());
}

int main() {
    std::cout << "Bot ishga tushdi..." << std::endl;
    long long last_update_id = 0;

    while (true) {
        std::string url = "https://api.telegram.org/bot" + BOT_TOKEN + "/getUpdates?offset=" + std::to_string(last_update_id + 1) + "&timeout=30";
        std::string resp = sendHttpRequest(url, "");
        
        try {
            auto j = json::parse(resp);
            if (j.contains("result")) {
                for (const auto& update : j["result"]) {
                    last_update_id = update["update_id"].get<long long>();

                    if (update.contains("business_message")) {
                        auto msg = update["business_message"];

                        if (msg.contains("from") && msg["from"].contains("is_bot")) {
                            if (msg["from"]["is_bot"].get<bool>()) {
                                std::cout << "Boshqa bot xabari e'tiborsiz qoldirildi." << std::endl;
                                continue;
                            }
                        }

                        if (msg.contains("text")) {
                            std::string user_text = msg["text"].get<std::string>();
                            std::string business_conn_id = msg.contains("business_connection_id") ? msg["business_connection_id"].get<std::string>() : "";
                            long long chat_id = msg["chat"]["id"].get<long long>();

                            std::string ai_reply = getAIResponse(user_text);
                            sendTelegramMessage(chat_id, ai_reply, business_conn_id);
                        }
                    }
                }
            }
        } catch (...) {}

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}

