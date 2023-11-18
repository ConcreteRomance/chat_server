#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>

#include <iostream>
#include <unistd.h>
#include <nlohmann/json.hpp>

#include "User.h"
#include "ChatRoom.h"
#include "system_message.h"
#include "system_type.h"
#include "ThreadPool.h"
#include "MessageFromClient.h"

#define JSON 0
#define PROTOBUF 1

using namespace nlohmann;

void respond(int clientSock, const std::string &ip, int port);

int messageFormat(const char *buf);

void sendMessageToClient(const json &jsonData, int clientSock);

std::vector<ChatRoom *> chatRooms;

std::vector<uint8_t> getMessageLengthBytes(std::size_t messageLength);

int main() {
    int passiveSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in sin{};
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(9120);
    if (bind(passiveSock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
        std::cerr << "bind() failed: " << strerror(errno) << std::endl;
        return 1;
    }

    if (listen(passiveSock, 10) < 0) {
        std::cerr << "listen() failed: " << strerror(errno) << std::endl;
        return 1;
    }

    memset(&sin, 0, sizeof(sin));
    unsigned int sin_len = sizeof(sin);

    ThreadPool pool(100);

    while (true) {
        int clientSock = accept(passiveSock, (struct sockaddr *) &sin, &sin_len);
        if (clientSock < 0) {
            std::cerr << "accept() failed: " << strerror(errno) << std::endl;
            return 1;
        }
        pool.enqueue([clientSock, sin] {
            respond(clientSock, inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
        });
    }
    close(passiveSock);
}

void respond(int clientSock, const std::string &ip, int port) {
    while (1) {
        std::cout << "respond 접근" << std::endl;
        char buf[65536];

        User *client = new User(clientSock, "(" + ip + ", " + std::to_string(port) + ")");
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

                if (type == CS_NAME) {
                    std::cout << "/name 접근" << std::endl;
                    ChatRoom *currentRoom = client->getChatRoom();

                    //json 변환
                    std::cout << "json 변환 접근" << std::endl;
                    client->setNickname(jsonMessage["name"]);
                    std::string respMessage = CHANGE_NAME(client->getNickname());
                    json jsonData;
                    jsonData["type"] = "SCSystemMessage";
                    jsonData["text"] = respMessage;

                    //json 보낼 준비
                    std::cout << "json 보낼 준비 접근" << std::endl;
                    std::string serializedData = jsonData.dump();
                    std::string respData;
                    const auto dataSize = serializedData.size();
                    auto messageBytes = getMessageLengthBytes(dataSize);
                    for (auto byte: messageBytes) {
                        respData += byte;
                    }
                    respData += serializedData;
                    const char *respBuf = respData.c_str();
                    int offset = 0;

                    //일단 나한테 보냄
                    std::cout << "보내기 접근" << std::endl;
                    std::cout << "respData: " << respData << std::endl;
                    while (offset < respData.size()) {
                        int numSend = send(clientSock, respBuf + offset, respData.size() - offset, 0);
                        if (respData.empty()) {
                            std::cerr << "send() failed: " << strerror(errno) << std::endl;
                        } else {
                            std::cout << "Sent: " << numSend << std::endl;
                            offset += numSend;
                        }
                    }

                    //모두에게 전송
                    if (currentRoom != nullptr) {
                        for (User *user: currentRoom->getUser()) {
                            int otherClientSock = user->getSock();
                            while (offset < respData.size()) {
                                int numSend = send(otherClientSock, respBuf + offset, respData.size() - offset, 0);
                                if (respData.empty()) {
                                    std::cerr << "send() failed: " << strerror(errno) << std::endl;
                                } else {
                                    std::cout << "Sent: " << numSend << std::endl;
                                    offset += numSend;
                                }
                            }
                        }
                    }
                } else if (type == CS_ROOMS) {

                    json jsonData;
                    if (chatRooms.empty()) {
                        std::string respMessage = NO_CHATROOM_ERROR;
                        jsonData = {
                                {"text", respMessage}
                        };
                    } else {
                        //json 변환
                        jsonData = json::array();
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
                    }

                    sendMessageToClient(jsonData, clientSock);

                } else if (type == CS_CREATE_ROOM) {
                    json jsonData;
                    ChatRoom *currentRoom = client->getChatRoom();
                    if (currentRoom != nullptr) {
                        std::string respMessage = CREATE_CHATROOM_ERROR;
                        jsonData = {
                                {"text", respMessage}
                        };
                    } else {
                        //chatRoom 생성 및 입장
                        std::string roomTitle = jsonMessage["title"];
                        auto *room = new ChatRoom(roomTitle, clientSock);
                        room->setUser(client);

                        //json 변환
                        std::string respMessage = JOIN_NOTIFYING_ME(roomTitle);
                        jsonData = {
                                {"text", respMessage}
                        };
                    }

                    sendMessageToClient(jsonData, clientSock);

                } else if (type == CS_JOIN_ROOM) {
                    json jsonData;
                    ChatRoom *currentRoom = client->getChatRoom();
                    if (currentRoom != nullptr) {
                        std::string respMessage = CHATROOM_ALREADY_JOINED_ERROR;
                        jsonData = {
                                {"text", respMessage}
                        };
                        sendMessageToClient(jsonData, clientSock);
                    } else {
                        bool isRoom = false;
                        for (auto chatRoom: chatRooms) {
                            if (jsonMessage["roomId"] == chatRoom->getRoomNum()) {
                                isRoom = true;
                                currentRoom = chatRoom;
                                break;
                            }
                        }
                        if (!isRoom) {
                            std::string respMessage = NOT_FOUND_CHATROOM_ERROR;
                            jsonData = {
                                    {"text", respMessage}
                            };
                            sendMessageToClient(jsonData, clientSock);
                        } else {
                            std::string respMessage = JOIN_NOTIFYING_ME(currentRoom->getTitle());
                            jsonData = {
                                    {"text", respMessage}
                            };

                            sendMessageToClient(jsonData, clientSock);

                            respMessage = JOIN_NOTIFYING_OTHER(client->getNickname());
                            jsonData = {
                                    {"text", respMessage}
                            };

                            //json 보낼 준비
                            std::string respData = jsonData.dump();
                            const char *respBuf = respData.c_str();
                            int offset = 0;

                            //나빼고 모두에게 전송
                            for (User *user: currentRoom->getUser()) {
                                if (user != client) {
                                    int otherClientSock = user->getSock();
                                    while (offset < respData.size()) {
                                        int numSend = send(otherClientSock, respBuf + offset,
                                                           respData.size() - offset,
                                                           0);
                                        if (respData.empty()) {
                                            std::cerr << "send() failed: " << strerror(errno) << std::endl;
                                        } else {
                                            std::cout << "Sent: " << numSend << std::endl;
                                            offset += numSend;
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else if (type == CS_LEAVE_ROOM) {
                    //대화방을 나가는 명령어입니다.
                    json jsonData;
                    ChatRoom *currentRoom = client->getChatRoom();
                    //대화방에 참여 중이지 않을 때 서버는 [시스템 메시지] 현재 대화방에 들어가 있지 않습니다. 메시지를 해당 유저에게 전송해야 됩니다.
                    if (currentRoom == nullptr) {
                        std::string respMessage = LEAVE_CHATROOM_ERROR;
                        jsonData = {
                                {"text", respMessage}
                        };
                        sendMessageToClient(jsonData, clientSock);
                        //나간 유저 본인에게는 [시스템 메시지] 방제[hello world] 대화 방에서 퇴장했습니다. 와 같은 메시지를 전송해야 됩니다.
                    } else {
                        client->leaveChatRoom();
                        std::string respMessage = LEAVE_NOTIFYING_ME(currentRoom->getTitle());
                        jsonData = {
                                {"text", respMessage}
                        };
                        sendMessageToClient(jsonData, clientSock);

                        respMessage = LEAVE_NOTIFYING_OTHER(client->getNickname());
                        jsonData = {
                                {"text", respMessage}
                        };

                        //json 보낼 준비
                        std::string respData = jsonData.dump();
                        const char *respBuf = respData.c_str();
                        int offset = 0;

                        //나빼고 모두에게 전송
                        for (User *user: currentRoom->getUser()) {
                            if (user != client) {
                                int otherClientSock = user->getSock();
                                while (offset < respData.size()) {
                                    int numSend = send(otherClientSock, respBuf + offset,
                                                       respData.size() - offset,
                                                       0);
                                    if (respData.empty()) {
                                        std::cerr << "send() failed: " << strerror(errno) << std::endl;
                                    } else {
                                        std::cout << "Sent: " << numSend << std::endl;
                                        offset += numSend;
                                    }
                                }
                            }
                        }
                    }
                } else if (type == CS_SHUTDOWN) {
                    close(clientSock);
                    return;
                } else if (type == CS_CHAT) {
                    json jsonData;
                    ChatRoom *currentRoom = client->getChatRoom();
                    if (currentRoom == nullptr) {
                        std::string respMessage = NOT_JOINED_CHATROOM_ERROR;
                        jsonData = {
                                {"text", respMessage}
                        };
                        sendMessageToClient(jsonData, clientSock);
                    } else {
                        std::string respMessage = jsonMessage["text"];
                        jsonData = {
                                {"member", client->getNickname()},
                                {"text",   respMessage}
                        };

                        //json 보낼 준비
                        std::string respData = jsonData.dump();
                        const char *respBuf = respData.c_str();
                        int offset = 0;

                        //나빼고 모두에게 전송
                        for (User *user: currentRoom->getUser()) {
                            if (user != client) {
                                int otherClientSock = user->getSock();
                                while (offset < respData.size()) {
                                    int numSend = send(otherClientSock, respBuf + offset,
                                                       respData.size() - offset,
                                                       0);
                                    if (respData.empty()) {
                                        std::cerr << "send() failed: " << strerror(errno) << std::endl;
                                    } else {
                                        std::cout << "Sent: " << numSend << std::endl;
                                        offset += numSend;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    //TODO:: ERROR HANDLER 필요
                }
                break;
        }
    }

}

int messageFormat(const char *buf) {
    if (!isprint(buf[0])) return JSON;
    return PROTOBUF;
}

void sendMessageToClient(const json &jsonData, int clientSock) {
    //json 보낼 준비
    std::string respData = jsonData.dump();
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

// 메시지의 길이를 2바이트로 변환하여 반환하는 함수
std::vector<uint8_t> getMessageLengthBytes(std::size_t messageLength) {
    std::vector<uint8_t> result(2);

    // 메시지 길이를 2바이트로 변환
    result[0] = static_cast<uint8_t>((messageLength >> 8) & 0xFF);
    result[1] = static_cast<uint8_t>(messageLength & 0xFF);

    return result;
}
