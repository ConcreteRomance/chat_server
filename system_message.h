
#ifndef CHAT_CLIENT_SYSTEM_MESSAGE_H
#define CHAT_CLIENT_SYSTEM_MESSAGE_H

#include <string>

//'/create'
//대화 방 생성 시, 이미 대화 방에 참여중일 때
static std::string CREATE_CHATROOM_ERROR
        = "대화 방에 있을 때는 방을 개설할 수 없습니다.";

//'/join'
//대화 방 입장 시, 본인에게 대화 방 이름 알림
inline std::string JOIN_NOTIFYING_ME(const std::string &title) {
    return "방제[" + title + "] 방에 입장했습니다.";
}

//대화 방 입장 시, 대화 방에 다른 유저들에게
inline std::string JOIN_NOTIFYING_OTHER(const std::string &name) {
    return "[" + name + "] 님이 입장했습니다.";
}

//다른 대화 방에 접근 시, 이미 대화 방에 있을 때
static std::string CHATROOM_ALREADY_JOINED_ERROR
        = "대화 방에 있을 때는 다른 방에 들어갈 수 없습니다.";

//대화 방에 접근 시, 해당 대화 방이 없을 때
static std::string NOT_FOUND_CHATROOM_ERROR
        = "대화 방이 존재하지 않습니다.";

//'/name'
//이름 변경 시, 본인이게 알림
inline std::string CHANGE_NAME(const std::string &name) {
    return "이름이 " + name + " 으로 변경되었습니다.";
}

//'/join'
//개설된 대화방이 없을 경우, 본인에게 알림
inline std::string NO_CHATROOM_ERROR
        = "개설된 대화방이 없습니다.";

//'/leave'
static std::string LEAVE_CHATROOM_ERROR
        = "현재 대화 방에 들어가 있지 않습니다.";

inline std::string LEAVE_NOTIFYING_ME(const std::string &title) {
    return "방제[" + title + "] 대화 방에서 퇴장했습니다.";
}

inline std::string LEAVE_NOTIFYING_OTHER(const std::string &name) {
    return "[" + name + "] 님이 퇴장했습니다.";
}

//etc
static std::string NOT_JOINED_CHATROOM_ERROR
        = "현재 대화 방에 들어가 있지 않습니다.";

#endif //CHAT_CLIENT_SYSTEM_MESSAGE_H
