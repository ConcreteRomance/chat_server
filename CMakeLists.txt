cmake_minimum_required(VERSION 3.26)
project(chat_client)

set(CMAKE_CXX_STANDARD 17)

add_executable(
        chat_client main.cpp
        ChatRoom.cpp
        ChatRoom.h
        User.cpp
        User.h
        traffic_setting.h
        system_message.h
        ThreadPool.h
        system_type.h
        JsonMessage.cpp
        JsonMessage.h
        Message.cpp
        Message.h
        MessageFromClient.cpp
        MessageFromClient.h
)
include(FetchContent)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
)

FetchContent_MakeAvailable(json)

target_link_libraries(chat_client PRIVATE nlohmann_json::nlohmann_json)

include_directories("/json/include/nlohmann")


