// Enhanced Chat Application in C++
// Includes Server and Client Code with unique IDs, timestamps,  exit command, and improved logging

#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 10

std::vector<int> clients;
std::map<int, std::string> clientIDs;
std::mutex client_mutex;
int clientCounter = 0;

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void broadcastMessage(const std::string& message, int sender) {
    std::lock_guard<std::mutex> lock(client_mutex);
    for (int client : clients) {
        if (client != sender) {
            send(client, message.c_str(), message.length(), 0);
        }
    }
}

void handleClient(int clientSocket) {
    char buffer[1024];
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        clientCounter++;
        clientIDs[clientSocket] = "Client " + std::to_string(clientCounter);
        clients.push_back(clientSocket);
        std::cout << "New client connected: " << clientIDs[clientSocket] << "\n";
        std::cout << "Active clients: " << clients.size() << "\n";
    }
    broadcastMessage(clientIDs[clientSocket] + " has joined the chat.\n", clientSocket);

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) {
            std::lock_guard<std::mutex> lock(client_mutex);
            std::cerr << clientIDs[clientSocket] << " disconnected.\n";
            broadcastMessage(clientIDs[clientSocket] + " has left the chat.\n", clientSocket);
            clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
            clientIDs.erase(clientSocket);
            close(clientSocket);
            std::cout << "Active clients: " << clients.size() << "\n";
            break;
        }
        std::string message(buffer);

        if (message == "/exit") {
            std::lock_guard<std::mutex> lock(client_mutex);
            std::cout << clientIDs[clientSocket] << " has exited the chat.\n";
            broadcastMessage(clientIDs[clientSocket] + " has left the chat.\n", clientSocket);
            clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
            clientIDs.erase(clientSocket);
            close(clientSocket);
            std::cout << "Active clients: " << clients.size() << "\n";
            break;
        }

        std::string timestamp = getCurrentTimestamp();
        std::string formattedMessage = "[" + timestamp + "] " + clientIDs[clientSocket] + ": " + message + "\n";
        std::cout << formattedMessage;
        broadcastMessage(formattedMessage, clientSocket);
    }
}

int main() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == 0) {
        std::cerr << "Socket creation failed!\n";
        return -1;
    }

    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed!\n";
        return -1;
    }

    if (listen(serverSocket, MAX_CLIENTS) < 0) {
        std::cerr << "Listen failed!\n";
        return -1;
    }

    std::cout << "Server is running on port " << PORT << "...\n";

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            std::cerr << "Failed to accept client!\n";
            continue;
        }
        std::thread(handleClient, clientSocket).detach();
    }

    close(serverSocket);
    return 0;
}

// Enhanced Client Code
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

void receiveMessages(int socket) {
    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(socket, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            std::cout << buffer;
        } else {
            std::cerr << "Disconnected from server.\n";
            close(socket);
            exit(0);
        }
    }
}

int main() {
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Socket creation failed!\n";
        return -1;
    }

    sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection to server failed!\n";
        return -1;
    }

    std::cout << "Connected to the server. Type messages to send (type /exit to disconnect):\n";

    std::thread(receiveMessages, clientSocket).detach();

    while (true) {
        std::string message;
        std::getline(std::cin, message);
        send(clientSocket, message.c_str(), message.length(), 0);
        if (message == "/exit") {
            std::cout << "Exiting...\n";
            close(clientSocket);
            break;
        }
    }

    return 0;
}
