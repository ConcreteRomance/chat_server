#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>

#include <iostream>
#include <unistd.h>
#include <set>
#include <nlohmann/json.hpp>
#include <utility>

#include "User.h"
#include "ChatRoom.h"
#include "system_message.h"
#include "system_type.h"
#include "ThreadPool.h"

#define JSON 0
#define PROTOBUF 1

using namespace nlohmann;

void respond(int clientSock, int passiveSock, const std::string &ip, int port);

int messageFormat(const char *buf);

void sendMessageToClient(const std::string &respData, int clientSock);

void sendMessageToOtherClient(std::string &respData, User *client);

std::vector<uint8_t> getMessageLengthBytes(std::size_t messageLength);

std::string toSystemMessage(std::string message);

std::string toChattingMessage(std::string name, std::string &text);

std::string toRoomsResultMessage();

std::string convertJsonToMessage(const json &jsonData);

ChatRoom *findChatRoom(const int &roomId);

void deleteRoom(ChatRoom *currentRoom);

std::vector<ChatRoom *> chatRooms;

std::set<int> clientSocks;

class AppMessage {
public:
    AppMessage(User *user, json jsonMessage, int clientSock, int passiveSock) {
        user_ = user;
        jsonMessage_ = std::move(jsonMessage);
        clientSock_ = clientSock;
    };

    User *user_;
    json jsonMessage_;
    int clientSock_;
    int passiveSock_;
};

typedef std::string MessageType;

typedef void (*MessageHandler)(const AppMessage *);

typedef std::map<MessageType, MessageHandler> HandlerMap;

void onName(const AppMessage *msg);

void onCreateRoom(const AppMessage *msg);

void onChat(const AppMessage *msg);

void onShutDown(const AppMessage *msg);

void onLeaveRoom(const AppMessage *msg);

void onJoinRoom(const AppMessage *msg);

void onRooms(const AppMessage *msg);

static HandlerMap handlers{
        {CS_NAME,        onName},
        {CS_CREATE_ROOM, onCreateRoom},
        {CS_CHAT,        onChat},
        {CS_SHUTDOWN,    onShutDown},
        {CS_LEAVE_ROOM,  onLeaveRoom},
        {CS_JOIN_ROOM,   onJoinRoom},
        {CS_ROOMS,       onRooms},
};

int main() {
    int numWorkThread = 100;
    std::cout << "Work Thread 개수를 지정해주세요(default = 100): " << std::endl;
    std::cin >> numWorkThread;

    int passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in sin{};
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(9119);
    if (bind(passiveSock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        std::cerr << "bind() failed: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Socket Binding Complete!" << std::endl;

    if (listen(passiveSock, 10) < 0) {
        std::cerr << "listen() failed: " << strerror(errno) << std::endl;
        return 1;
    }

    std::cout << "Listening.." << std::endl;

    memset(&sin, 0, sizeof(sin));
    unsigned int sin_len = sizeof(sin);

    ThreadPool pool(numWorkThread);

    while (true) {
        fd_set rset;
        FD_ZERO(&rset);

        FD_SET(passiveSock, &rset);
        int maxFd = passiveSock;

        for (auto sock: clientSocks) {
            FD_SET(sock, &rset);
            if (sock > maxFd) maxFd = sock;
        }

        int numReady = select(maxFd + 1, &rset, nullptr, nullptr, nullptr);
        if (numReady < 0) {
            std::cerr << "select() failed: " << strerror(errno) << std::endl;
            continue;
        } else if (numReady == 0) {
            continue;
        }

        if (FD_ISSET(passiveSock, &rset)) {
            memset(&sin, 0, sizeof(sin));
            unsigned int sin_len = sizeof(sin);
            int clientSock = accept(passiveSock, (struct sockaddr *) &sin, &sin_len);
            if (clientSock < 0) {
                std::cerr << "accept() failed: " << strerror(errno) << std::endl;
            } else {
                clientSocks.insert(clientSock);
            }
            pool.enqueue([clientSock, passiveSock, sin] {
                respond(clientSock, passiveSock, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
            });
        }
    }
}

void respond(int clientSock, int passiveSock, const std::string &ip, int port) {
    std::cout << "respond 접근" << std::endl;
    User *client = new User(clientSock, "(" + ip + ", " + std::to_string(port) + ")");
    while (true) {
        char buf[65536];
        int numRecv = recv(clientSock, buf, sizeof(buf), 0);
        std::string trimBuf(buf + 2, numRecv - 2);
        std::cout << "Recv: " << numRecv << std::endl;
        std::cout << "messageFormat(buf): " << messageFormat(buf) << std::endl;

        switch (messageFormat(buf)) {
            case JSON:
                std::cout << "JSON 입장" << std::endl;
                //request 받음
                json jsonMessage = json::parse(trimBuf);
                std::cout << "parsing 완료" << std::endl;
                std::string type = jsonMessage["type"];
                std::cout << "type: " << type << std::endl;

                auto *msg = new AppMessage(client, jsonMessage, clientSock, passiveSock);

                if (handlers.find(type) != handlers.end()) {
                    handlers[type](msg);
                } else {
                    std::cerr << "Invalid type: " << type << std::endl;
                }
        }
    }
}

void onName(const AppMessage *msg) {
    User *client = msg->user_;
    json jsonMessage = msg->jsonMessage_;
    int clientSock = msg->clientSock_;

    std::cout << "/name 접근" << std::endl;
    ChatRoom *currentRoom = client->getChatRoom();

    client->setNickname(jsonMessage["name"]);   //client의 name 재설정

    //보낼 메시지 생성
    auto respData = toSystemMessage(CHANGE_NAME(client->getNickname()));
    //나에게 전송
    sendMessageToClient(respData, clientSock);
    //모두에게 전송
    if (currentRoom != nullptr) sendMessageToOtherClient(respData, client);
}

void onCreateRoom(const AppMessage *msg) {
    User *client = msg->user_;
    json jsonMessage = msg->jsonMessage_;
    int clientSock = msg->clientSock_;

    std::cout << "/create 접근" << std::endl;
    ChatRoom *currentRoom = client->getChatRoom();
    std::string respData;
    if (currentRoom != nullptr) {   //현재 참여중인 방이 있을 경우
        respData = toSystemMessage(CREATE_CHATROOM_ERROR);
    } else {
        //chatRoom 생성 및 입장
        std::string roomTitle = jsonMessage["title"];
        chatRooms.emplace_back(new ChatRoom(roomTitle, clientSock, client));    //방 생성 & 방에 입장시키ㅣㄱ
        respData = toSystemMessage(JOIN_NOTIFYING_ME(roomTitle));
    }
    sendMessageToClient(respData, clientSock);

}

void onChat(const AppMessage *msg) {
    User *client = msg->user_;
    json jsonMessage = msg->jsonMessage_;
    int clientSock = msg->clientSock_;

    std::cout << "chat 접근" << std::endl;
    json jsonData;
    std::string respData;
    ChatRoom *currentRoom = client->getChatRoom();
    if (currentRoom == nullptr) {
        respData = toSystemMessage(NOT_JOINED_CHATROOM_ERROR);
        sendMessageToClient(respData, clientSock);
    } else {
        std::string respMessage = jsonMessage["text"];
        respData = toChattingMessage(client->getNickname(), respMessage);
        sendMessageToOtherClient(respData, client);
    }
}

void onShutDown(const AppMessage *msg) {
    int passiveSock = msg->passiveSock_;

    for (auto sock: clientSocks) {
        close(sock);
    }
    close(passiveSock);
    exit(0);
}

void onLeaveRoom(const AppMessage *msg) {
    User *client = msg->user_;
    json jsonMessage = msg->jsonMessage_;
    int clientSock = msg->clientSock_;

    std::cout << "/leave 접근" << std::endl;
    json jsonData;
    std::string respData;
    ChatRoom *currentRoom = client->getChatRoom();
    if (currentRoom == nullptr) {
        respData = toSystemMessage(LEAVE_CHATROOM_ERROR);
        sendMessageToClient(respData, clientSock);
    } else {
        //나에게 보내기
        respData = toSystemMessage(LEAVE_NOTIFYING_ME(currentRoom->getTitle()));
        sendMessageToClient(respData, clientSock);
        //나머지에게 보내기
        respData = toSystemMessage(LEAVE_NOTIFYING_OTHER(client->getNickname()));
        sendMessageToOtherClient(respData, client);
        client->leaveChatRoom();    //방에서 나가기
        if (currentRoom->getUser().empty()) deleteRoom(currentRoom);
    }
}

void onJoinRoom(const AppMessage *msg) {
    User *client = msg->user_;
    json jsonMessage = msg->jsonMessage_;
    int clientSock = msg->clientSock_;

    std::cout << "/join 접근" << std::endl;
    std::string respData;
    json jsonData;
    ChatRoom *currentRoom = client->getChatRoom();
    if (currentRoom != nullptr) {
        respData = toSystemMessage(CHATROOM_ALREADY_JOINED_ERROR);
        sendMessageToClient(respData, clientSock);
    } else {
        ChatRoom *findRoom = findChatRoom(jsonMessage["roomId"]);
        if (findRoom == nullptr) {
            respData = toSystemMessage(NOT_FOUND_CHATROOM_ERROR);
            sendMessageToClient(respData, clientSock);
        } else {
            findRoom->setUser(client);   //방 입장
            //나에게 보내기
            respData = toSystemMessage(JOIN_NOTIFYING_ME(findRoom->getTitle()));
            sendMessageToClient(respData, clientSock);
            //나머지에게 보내기
            respData = toSystemMessage(JOIN_NOTIFYING_OTHER(client->getNickname()));
            sendMessageToOtherClient(respData, client);
        }
    }
}

void onRooms(const AppMessage *msg) {
    json jsonMessage = msg->jsonMessage_;
    int clientSock = msg->clientSock_;

    std::cout << "/room 접근" << std::endl;
    std::string respData;
    if (chatRooms.empty()) {
        respData = toSystemMessage(NO_CHATROOM_ERROR);
    } else {
        respData = toRoomsResultMessage();
        std::cout << respData << std::endl;
    }
    sendMessageToClient(respData, clientSock);
}

int messageFormat(const char *buf) {
    if (!isprint(buf[0])) return JSON;
    return PROTOBUF;
}

//클라이언트한테 메시지 보내기
void sendMessageToClient(const std::string &respData, int clientSock) {
    //json 보낼 준비
    const char *respBuf = respData.c_str();
    int offset = 0;

    //일단 나한테 보냄
    while (offset < respData.size()) {
        int numSend = send(clientSock, respBuf + offset, respData.size() - offset, 0);
        if (respData.empty()) {
            std::cerr << "send() failed: " << strerror(errno) << std::endl;
        } else {
            std::cout << "Sent: " << numSend << std::endl;
            offset += numSend;
        }
    }
}

//나를 제외한 클라이언트에게 메시지 보내기
void sendMessageToOtherClient(std::string &respData, User *client) {
    ChatRoom *currentRoom = client->getChatRoom();
    for (auto user: currentRoom->getUser()) {
        if (user->getSock() != client->getSock()) {
            sendMessageToClient(respData, user->getSock());
        }
    }
}

// 메시지의 길이를 2바이트로 변환하여 반환하는 함수
std::vector<uint8_t> getMessageLengthBytes(std::size_t messageLength) {
    std::vector<uint8_t> result(2);

    // 메시지 길이를 2바이트로 변환
    result[0] = static_cast<uint8_t>((messageLength >> 8) & 0xFF);
    result[1] = static_cast<uint8_t>(messageLength & 0xFF);

    return result;
}

// 시스템 메시지 만들기
std::string toSystemMessage(std::string message) {
    json jsonData = {{"type", SC_SYSTEM_MESSAGE},
                     {"text", message}};

    return convertJsonToMessage(jsonData);
}

// 채팅 메시지 만들기
std::string toChattingMessage(std::string name, std::string &text) {
    json jsonData = {{"type",   SC_CHAT},
                     {"member", name},
                     {"text",   text}};

    return convertJsonToMessage(jsonData);
}

std::string toRoomsResultMessage() {
    json jsonData;
    jsonData["type"] = SC_ROOMS_RESULT;
    jsonData["rooms"] = json::array();
    for (ChatRoom *room: chatRooms) {
        json jsonRoom;
        jsonRoom["roomId"] = room->getRoomNum();
        jsonRoom["title"] = room->getTitle();
        jsonRoom["members"] = json::array();
        for (User *user: room->getUser()) {
            jsonRoom["members"].push_back(user->getNickname());
        }
        jsonData["rooms"].push_back(jsonRoom);
    }

    return convertJsonToMessage(jsonData);
}

// json 을 message 로
std::string convertJsonToMessage(const json &jsonData) {
    std::string serializedData = jsonData.dump();
    std::size_t messageLength = serializedData.size();
    auto messageBytes = getMessageLengthBytes(messageLength);
    std::string message;
    for (auto byte: messageBytes) {
        message += byte;
    }
    message += serializedData;
    return message;
}

//방 찾기
ChatRoom *findChatRoom(const int &roomId) {
    bool isRoom = false;
    for (auto chatRoom: chatRooms) {
        if (roomId == chatRoom->getRoomNum()) {
            isRoom = true;
            return chatRoom;
        }
    }
    return nullptr;
}

//방 삭제
void deleteRoom(ChatRoom *currentRoom) {
    for (int i = 0; i < chatRooms.size(); i++) {
        if (chatRooms[i] == currentRoom) {
            chatRooms.erase(chatRooms.begin() + i, chatRooms.begin() + i + 1);
            break;
        }
    }
    delete currentRoom;
}


