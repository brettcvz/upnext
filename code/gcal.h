/**
 *  @filename   :   screen.h
 *  @brief      :   Header file for Screen abstraction of cairo surface for epaper display
 *  @author     :   Brett van Zuiden
 *  
 */

#ifndef GCAL_H
#define GCAL_H

#include <string>
#include "../lib/json.hpp"
#include "restclient-cpp/connection.h"
using namespace std;
using json = nlohmann::json;

class GoogleCalendar {
public:
    GoogleCalendar();

    void SetCredentials(string clientID, string clientSecret);
    void SetAuthToken(string newToken, string newRefresh);
    void GetInstalledAppTokenForCode(string code);
    void RefreshAuthToken();
    void RequestInstalledAppToken();
    json GetEventsBetween(string calendarID, string timeMin, string timeMax);
    json MakeGetRequest(string path);

private:
    string clientID;
    string clientSecret;
    string authToken;
    string refreshToken;
    RestClient::Connection* gcalApiConnection;
};

#endif
