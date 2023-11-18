#include "ChatRoom.h"

ChatRoom::ChatRoom() = default;

ChatRoom::ChatRoom(std::string title, int roomNum) {
    title_ = title;
    roomNum_ = roomNum;
}

int ChatRoom::getRoomNum() {
    return roomNum_;
}

std::string ChatRoom::getTitle() {
    return title_;
}

std::vector<User *> ChatRoom::getUser() {
    return users_;
}

void ChatRoom::setTitle(std::string title) {
    title_ = title;
}

void ChatRoom::setUser(User *user) {
    users_.push_back(user);
    user->setChatRoom(this);
}

void ChatRoom::dropUser(int sock) {
    for (int i = 0; i < users_.size(); i++) {
        if (users_[i]->getSock() == sock) {
            users_.erase(users_.begin() + i, users_.begin() + i + 1);
            break;
        }
    }
}
