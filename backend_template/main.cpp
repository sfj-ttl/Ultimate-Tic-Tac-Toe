#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "game_function.cpp"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

struct Request {
    string method;
    string path;
    string body;
};

static game current_game;

bool fileExists(const string& path) {
    ifstream in(path, ios::binary);
    return static_cast<bool>(in);
}

string extensionOf(const string& path) {
    size_t dot = path.find_last_of('.');
    return dot == string::npos ? "" : path.substr(dot);
}

string contentTypeOf(const string& path) {
    string ext = extensionOf(path);
    if (ext == ".html") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    return "text/plain; charset=utf-8";
}

string readFile(const string& path) {
    ifstream in(path, ios::binary);
    ostringstream out;
    out << in.rdbuf();
    return out.str();
}

string frontendRoot() {
    for (const string& path : {"frontend\\", "..\\frontend\\", "..\\..\\frontend\\"}) {
        if (fileExists(path + "index.html")) return path;
    }
    return "";
}

string httpResponse(int code, const string& contentType, const string& body) {
    ostringstream out;
    out << "HTTP/1.1 " << code << " "
        << (code == 200 ? "OK" : code == 400 ? "Bad Request" : "Not Found") << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Access-Control-Allow-Headers: Content-Type\r\n"
        << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    return out.str();
}

int intField(const string& body, const string& name) {
    size_t pos = body.find("\"" + name + "\"");
    if (pos == string::npos) pos = body.find(name + "=");
    if (pos == string::npos) return -1;

    pos = body.find_first_of("-0123456789", pos + name.size());
    if (pos == string::npos) return -1;

    int sign = 1;
    if (body[pos] == '-') {
        sign = -1;
        ++pos;
    }

    int value = 0;
    while (pos < body.size() && isdigit(static_cast<unsigned char>(body[pos]))) {
        value = value * 10 + body[pos] - '0';
        ++pos;
    }
    return value * sign;
}

string handleRequest(const Request& request) {
    if (request.method == "OPTIONS") {
        return httpResponse(200, "application/json; charset=utf-8", "{}");
    }
    if (request.method == "GET" && request.path == "/api/state") {
        return httpResponse(200, "application/json; charset=utf-8", current_game.tojson());
    }
    if (request.method == "POST" && request.path == "/api/new") {
        current_game.reset();
        return httpResponse(200, "application/json; charset=utf-8", current_game.tojson());
    }
    if (request.method == "POST" && request.path == "/api/move") {
        string error;
        int row = intField(request.body, "row");
        int col = intField(request.body, "col");
        if (!current_game.makemove(row, col, error)) {
            return httpResponse(400, "application/json; charset=utf-8", current_game.errorjson(error));
        }
        return httpResponse(200, "application/json; charset=utf-8", current_game.tojson());
    }
    if (request.method == "POST" && request.path == "/api/ai-move") {
        string error;
        int level = intField(request.body, "level");
        if (level < 1 || level > 3) level = 2;
        if (!current_game.makeaimove(level, error)) {
            return httpResponse(400, "application/json; charset=utf-8", current_game.errorjson(error));
        }
        return httpResponse(200, "application/json; charset=utf-8", current_game.tojson());
    }

    if (request.method == "GET") {
        string root = frontendRoot();
        if (root.empty()) {
            return httpResponse(404, "text/plain; charset=utf-8", "frontend not found");
        }

        string rel = request.path == "/" ? "/index.html" : request.path;
        if (rel.find("..") != string::npos) {
            return httpResponse(404, "text/plain; charset=utf-8", "not found");
        }
        if (!rel.empty() && rel[0] == '/') rel.erase(rel.begin());
        for (char& ch : rel) {
            if (ch == '/') ch = '\\';
        }

        string file = root + rel;
        if (!fileExists(file)) {
            return httpResponse(404, "text/plain; charset=utf-8", "not found");
        }
        return httpResponse(200, contentTypeOf(file), readFile(file));
    }

    return httpResponse(404, "text/plain; charset=utf-8", "not found");
}

Request parseRequest(const string& raw) {
    Request request;
    istringstream firstLine(raw.substr(0, raw.find("\r\n")));
    firstLine >> request.method >> request.path;

    size_t query = request.path.find('?');
    if (query != string::npos) request.path = request.path.substr(0, query);

    size_t bodyStart = raw.find("\r\n\r\n");
    if (bodyStart != string::npos) request.body = raw.substr(bodyStart + 4);
    return request;
}

string receiveRequest(SOCKET client) {
    string raw;
    char buffer[4096];
    int bytes = 0;

    do {
        bytes = recv(client, buffer, sizeof(buffer), 0);
        if (bytes > 0) raw.append(buffer, bytes);

        size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd != string::npos) {
            size_t lengthPos = raw.find("Content-Length:");
            if (lengthPos == string::npos) break;

            int length = stoi(raw.substr(lengthPos + 15));
            if (raw.size() >= headerEnd + 4 + static_cast<size_t>(length)) break;
        }
    } while (bytes > 0);

    return raw;
}

void sendAll(SOCKET client, const string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int bytes = send(client, data.data() + sent, static_cast<int>(data.size() - sent), 0);
        if (bytes <= 0) break;
        sent += bytes;
    }
}

int main() {
    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) return 1;

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(8080);

    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        cerr << "bind failed, port 8080 may be in use\n";
        return 1;
    }

    listen(server, SOMAXCONN);
    cout << "Ultimate Tic-Tac-Toe server running at http://localhost:8080\n";
    cout << "Press Ctrl+C to stop.\n";

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        int receiveTimeoutMs = 2000;
        setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receiveTimeoutMs), sizeof(receiveTimeoutMs));

        string raw = receiveRequest(client);
        if (!raw.empty()) sendAll(client, handleRequest(parseRequest(raw)));
        closesocket(client);
    }
}
