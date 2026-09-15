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

// HTTP POST so'rov funksiyasi
std::string sendHttpRequest(const std::string& url, const std::string& json_data) {
    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        struct curl_slist* headers = curl_slist_append(NULL, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    return response;
}

// Gemini API orqali BEPUL AI javobini olish
std::string getAIResponse(const std::string& prompt) {
    // gemini-3.6-flash modeliga almashtiramiz
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.6-flash:generateContent?key=" + GEMINI_API_KEY;
    
    json body = {
        {"contents", json::array({
            {{"parts", json::array({{{"text", prompt}}})}}
        })}
    };

    std::string res = sendHttpRequest(url, body.dump());

    try {
        auto parsed = json::parse(res);
        if (parsed.contains("error")) {
            std::cout << "[GEMINI XATO]: " << parsed["error"]["message"] << std::endl;
            return "AI Xatosi: " + parsed["error"]["message"].get<std::string>();
        }
        if (parsed.contains("candidates") && !parsed["candidates"].empty()) {
            return parsed["candidates"][0]["content"]["parts"][0]["text"];
        }
    } catch (...) {}

    return "Javob olishda noma'lum xatolik yuz berdi.";
}


int main() {
    std::cout << "Gemini AI Business Bot ishga tushdi..." << std::endl;
    long long offset = 0;

    while (true) {
        std::string updates = sendHttpRequest("https://api.telegram.org/bot" + BOT_TOKEN + "/getUpdates", 
            json({{"offset", offset}, {"timeout", 20}, {"allowed_updates", {"business_message"}}}).dump());
        
        try {
            auto data = json::parse(updates);
            for (const auto& update : data.value("result", json::array())) {
                offset = update["update_id"].get<long long>() + 1;

                if (update.contains("business_message")) {
                    auto msg = update["business_message"];
                    if (msg.contains("text")) {
                        std::string user_text = msg["text"];
                        std::cout << "[XABAR]: " << user_text << std::endl;
                        
                        std::string reply = getAIResponse(user_text);
                        
                        json reply_body = {
                            {"chat_id", msg["chat"]["id"]},
                            {"text", reply},
                            {"business_connection_id", msg.value("business_connection_id", "")}
                        };
                        sendHttpRequest("https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage", reply_body.dump());
                    }
                }
            }
        } catch (...) {}

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    return 0;
}

