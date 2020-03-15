/**
 *  @filename   :   gcal.cpp
 *  @brief      :   Library for interfacing with the google calendar api
 *  @author     :   Brett van Zuiden
 */

#include <stdlib.h>
#include <iostream>
#include <string.h>
#include "restclient-cpp/connection.h"
#include "restclient-cpp/restclient.h"

#include "gcal.h"
using namespace std;
using json = nlohmann::json;

const string API_BASE_URL = "https://www.googleapis.com/calendar/v3";
const string AUTH_BASE_URL = "https://accounts.google.com/o/oauth2/auth";
const string TOKEN_URL = "https://accounts.google.com/o/oauth2/token";
const string REDIRECT_URI = "urn:ietf:wg:oauth:2.0:oob";

GoogleCalendar::GoogleCalendar() {
  string authToken;
  string refreshToken;
  string clientID;
  string clientSecret;

  //curl_global_init(CURL_GLOBAL_DEFAULT);
  RestClient::init();
  gcalApiConnection = new RestClient::Connection(API_BASE_URL);
};

void GoogleCalendar::SetCredentials(string newClientID, string newClientSecret) {
  clientID = newClientID;
  clientSecret = newClientSecret;
}

void GoogleCalendar::SetAuthToken(string newToken, string newRefreshToken) {
  authToken = newToken;
  refreshToken = newRefreshToken;
}

void GoogleCalendar::GetInstalledAppTokenForCode(string code) {
  string data = "code=" + code;
  data += "&client_id=" + clientID;
  data += "&client_secret=" + clientSecret;
  data += "&redirect_uri=" + REDIRECT_URI;
  data += "&grant_type=authorization_code";

  /*
  CURL *curl;
  char errbuff [CURL_ERROR_SIZE];
  errbuf[0] = 0;

  curl = curl_easy_init();
  if(!curl) {
    cout << "Error initalizing curl, aborting" << endl;
    curl_easy_cleanup(curl);
    return;
  }

  curl_easy_setopt(curl, CURLOPT_URL, TOKEN_URL);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

  CURLcode res;
  res = curl_easy_perform(curl);

  if(res != CURLE_OK) {
    cout << "Error fetching token: " << curl_easy_strerror(res) << endl;
    cout << "Error details: " << errbuff << endl;
    curl_easy_cleanup(curl);
    return;
  }

  long response_code;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

  curl_easy_cleanup(curl);
  */

  RestClient::Response r = RestClient::post(TOKEN_URL, "application/x-www-form-urlencoded", data);
  if (r.code != 200) {
    cout << "Error fetching token:" << endl;
    cout << "Error Code: " << r.code << endl;
    cout << r.body << endl;
    return;
  }
  json token = json::parse(r.body);
  SetAuthToken(token["access_token"], token["refresh_token"]);
  cout << "Access token: " << authToken << endl;
  cout << "Refresh token: " << refreshToken << endl;
}

void GoogleCalendar::RefreshAuthToken() {
  string data = "refresh_token=" + refreshToken;
  data += "&client_id=" + clientID;
  data += "&client_secret=" + clientSecret;
  data += "&grant_type=refresh_token";
  cout << "Refreshing google calendar access token" << endl;

  RestClient::Response r = RestClient::post(TOKEN_URL, "application/x-www-form-urlencoded", data);
  if (r.code != 200) {
    cout << "Error refreshing token:" << endl;
    cout << "Error Code: " << r.code << endl;
    cout << r.body << endl;
    return;
  }
  json token = json::parse(r.body);
  cout << "New access token: " << token["access_token"] << endl;
  SetAuthToken(token["access_token"], refreshToken);
}

void GoogleCalendar::RequestInstalledAppToken() {
  // urlencode(https://www.googleapis.com/auth/calendar.readonly)
  string scopes = "https%3A%2F%2Fwww.googleapis.com%2Fauth%2Fcalendar.readonly";
  string authURL = AUTH_BASE_URL + "?access_type=offline";
  authURL += "&scope=" + scopes;
  authURL += "&response_type=code";
  authURL += "&client_id=" + clientID;
  authURL += "&redirect_uri=" + REDIRECT_URI;

  cout << "Authorize this app by visiting this url: " << authURL << endl;

  string accessCode;
  cout << "Enter the code from that page: ";
  getline (cin, accessCode);

  GetInstalledAppTokenForCode(accessCode);
}

json GoogleCalendar::GetEventsBetween(string calendarID, string timeMin, string timeMax) {
  string eventPath = "/calendars/" + calendarID + "/events"; 
  eventPath += "?orderBy=startTime";
  eventPath += "&singleEvents=true";
  eventPath += "&timeMin=" + timeMin;
  eventPath += "&timeMax=" + timeMax;
  eventPath += "&maxAttendees=1";

  cout << "Events from: " << timeMin;
  cout << " to: " << timeMax << endl;

  json data = MakeGetRequest(eventPath);

  return data["items"];
}

json GoogleCalendar::MakeGetRequest(string path) {
  RestClient::HeaderFields headers;
  headers["Authorization"] = "Bearer " + authToken;
  gcalApiConnection->SetHeaders(headers);

  RestClient::Response r = gcalApiConnection->get(path);
  if (r.code == 401 || r.code == 403) {
    // Refresh auth token, retry
    RefreshAuthToken();
    headers["Authorization"] = "Bearer " + authToken;
    gcalApiConnection->SetHeaders(headers);
    r = gcalApiConnection->get(path);
  }

  json resp;
  if (r.code != 200) {
    cout << "Error executing request:" << endl;
    cout << "Error Code: " << r.code << endl;
    cout << r.body << endl;
    return resp;
  }
  resp = json::parse(r.body);
  return resp;
}
