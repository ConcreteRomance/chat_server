#ifndef CHAT_CLIENT_USER_H
#define CHAT_CLIENT_USER_H

#include <string>

class ChatRoom;
class User {
public:
    //Constructor
    User();

    User(int sock, std::string nickname_);

    //Getter
    int getSock();

    std::string getNickname();

    ChatRoom* getChatRoom();

    //Setter
    void setSock(int sock);

    void setNickname(std::string nickname);

    void setChatRoom(ChatRoom *chatRoom);

    void leaveChatRoom();

private:
    int sock_;
    std::string nickname_;
    ChatRoom *chatRoom_ = nullptr;
};

#endif //CHAT_CLIENT_USER_H
