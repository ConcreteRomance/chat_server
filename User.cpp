#include "User.h"
#include "ChatRoom.h"

User::User() = default;

User::User(int sock, std::string nickname){
    sock_ = sock;
    nickname_ = nickname;
}

int User::getSock() {
    return sock_;
}

std::string User::getNickname() {
    return nickname_;
}

ChatRoom* User::getChatRoom() {
    return chatRoom_;
}

void User::setSock(int sock){
    sock_ = sock;
}

void User::setNickname(std::string nickname) {
    nickname_ = nickname;
}

void User::setChatRoom(ChatRoom *chatRoom) {
    chatRoom_ = chatRoom;
}

void User::leaveChatRoom(){
    chatRoom_->dropUser(this->getSock());
    chatRoom_ = nullptr;
}


