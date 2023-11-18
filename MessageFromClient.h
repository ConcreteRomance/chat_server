#ifndef CHAT_CLIENT_MESSAGEFROMCLIENT_H
#define CHAT_CLIENT_MESSAGEFROMCLIENT_H

#include <string>

class MessageFromClient {
public:
    MessageFromClient(std::string type, std::string message);

private:
    std::string type_;
    std::string message_;
};


#endif //CHAT_CLIENT_MESSAGEFROMCLIENT_H
