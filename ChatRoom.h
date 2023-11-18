#ifndef CHAT_CLIENT_CHATROOM_H
#define CHAT_CLIENT_CHATROOM_H

#include "traffic_setting.h"
#include "User.h"
#include <string>

class ChatRoom {
public:
    //Constructor
    ChatRoom();

    ChatRoom(std::string title, int roomNum);

    //Getter
    int getRoomNum();

    std::string getTitle();

    std::vector<User*> getUser();

    //Setter

    void setTitle(std::string title);

    void setUser(User *user);

    void dropUser(int sock);

private:
    int roomNum_ = 0;
    std::string title_;
    std::vector<User*> users_;
};

#endif //CHAT_CLIENT_CHATROOM_H
