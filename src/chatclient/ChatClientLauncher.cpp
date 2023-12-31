#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <functional>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "group.h"
#include "user.h"
#include "public.h"
#include "clientinit.h"
#include "qq.pb.h"

using namespace std;

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间（聊天信息需要添加时间信息）
std::string getCurrentTime();
// 主聊天页面程序
void mainMenu(int);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv)
{

    ClientInit::Init(argc, argv);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(EXIT_FAILURE);
    }

    // 填写client需要连接的connectserver信息ip+port
    sockaddr_in connectserver_addr;
    memset(&connectserver_addr, 0, sizeof(sockaddr_in));

    connectserver_addr.sin_family = AF_INET;
    connectserver_addr.sin_addr.s_addr = inet_addr(ClientInit::GetInstance().GetConfig().Load("connectserverip").c_str());
    connectserver_addr.sin_port = htons(stoi(ClientInit::GetInstance().GetConfig().Load("connectserverport")));

    // client和connectserver进行连接
    if (-1 == connect(clientfd, (sockaddr *)&connectserver_addr, sizeof(sockaddr_in)))
    {
        cerr << "connect connectserver error" << endl;
        close(clientfd);
        exit(EXIT_FAILURE);
    }

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功，启动接收子线程
    std::thread readTask(readTaskHandler, clientfd); // pthread_create
    readTask.detach();                               // pthread_detach

    // main线程用于接收用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            std::string userid;
            std::string password;
            cout << "userid:";
            getline(cin, userid);
            cout << "password:";
            getline(cin, password);

            fixbug::LoginRequest loginrequest;
            loginrequest.set_userid(userid);
            loginrequest.set_password(password);

            fixbug::protobuffer buffer;
            buffer.set_protobuftype(LOGIN);
            buffer.set_protobufstr(loginrequest.SerializeAsString());

            g_isLoginSuccess = false;

            string request = buffer.SerializeAsString();

            // 发送给登录服务器
            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里

            if (g_isLoginSuccess)
            {
                {
                    g_currentUser.setUserId(userid);
                    g_currentUser.setUserName("zlw");

                    fixbug::GetFriendListRequest getfriendlistrequest;
                    getfriendlistrequest.set_userid(userid);

                    fixbug::protobuffer buffer;
                    buffer.set_protobuftype(GET_FRIENDLIST);
                    buffer.set_protobufstr(getfriendlistrequest.SerializeAsString());

                    string request = buffer.SerializeAsString();

                    // 发送给登录服务器
                    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                    if (len == -1)
                    {
                        cerr << "send getfriendlist msg error:" << request << endl;
                    }
                    sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里
                }

                {
                    fixbug::GetGroupListRequest getgrouplistrequest;
                    getgrouplistrequest.set_userid(userid);

                    fixbug::protobuffer buffer;
                    buffer.set_protobuftype(GET_GROUPLIST);
                    buffer.set_protobufstr(getgrouplistrequest.SerializeAsString());

                    string request = buffer.SerializeAsString();

                    // 发送给登录服务器
                    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                    if (len == -1)
                    {
                        cerr << "send getgrouplist msg error:" << request << endl;
                    }
                    sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里
                }
                {
                    // 进入聊天主菜单页面
                    showCurrentUserData();

                    fixbug::GetOfflineMsgRequest getofflinemsgrequest;
                    getofflinemsgrequest.set_userid(userid);

                    fixbug::protobuffer buffer;
                    buffer.set_protobuftype(GET_OFFLINEMSG);
                    buffer.set_protobufstr(getofflinemsgrequest.SerializeAsString());

                    string request = buffer.SerializeAsString();

                    // 发送给登录服务器
                    int len3 = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                    if (len3 == -1)
                    {
                        cerr << "send getofflinemsg msg error:" << request << endl;
                    }
                    sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里

                    isMainMenuRunning = true;
                    mainMenu(clientfd);
                }
            }
        }
        break;
        case 2: // register业务
        {
            std::string username;
            std::string password;
            cout << "username:";
            getline(cin, username);
            cout << "password:";
            getline(cin, password);

            fixbug::RegisterRequest registerrequest;
            registerrequest.set_username(username);
            registerrequest.set_password(password);

            fixbug::protobuffer buffer;
            buffer.set_protobuftype(REG);
            buffer.set_protobufstr(registerrequest.SerializeAsString());

            string request = buffer.SerializeAsString();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，子线程处理完注册消息会通知
        }
        break;
        case 3: // quit业务
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// 处理登录的响应逻辑
void doLoginResponse(fixbug::LoginResponse loginresponse)
{
    if (!loginresponse.result().success()) // 登录失败
    {
        cerr << loginresponse.result().errmsg() << endl;
        g_isLoginSuccess = false;
    }
    else // 登录成功
    {
        g_isLoginSuccess = true;
    }
}

// 处理注册的响应逻辑
void doRegisterResponse(fixbug::RegisterResponse registerresponse)
{
    if (!registerresponse.result().success()) // 注册失败
    {
        cerr << "name is already exist, register error!" << endl;
    }
    else // 注册成功
    {
        cout << "name register success, userid is " << registerresponse.userid()
             << ", do not forget it!" << endl;
    }
}

// 子线程 - 接收线程
void readTaskHandler(int clientfd)
{
    for (;;)
    {
        char buffer[2048] = {0};
        int len = recv(clientfd, buffer, 2048, 0); // 阻塞了

        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(EXIT_FAILURE);
        }

        string buf(buffer);
        fixbug::protobuffer pbuf;
        bool pres = pbuf.ParseFromString(buf);
        if (pres)
        {
            cout << "解析成功" << endl;
        }
        else
        {
            cout << "解析失败" << endl;
        }

        // 从字符流中读取前4个字节的内容
        uint32_t msgtype = pbuf.protobuftype();
        buf = pbuf.protobufstr();

        // 登录响应消息
        if (LOGIN_ACK == msgtype)
        {
            fixbug::LoginResponse loginresponse;
            loginresponse.ParseFromString(buf);
            doLoginResponse(loginresponse); // 处理登录响应的业务逻辑
            sem_post(&rwsem);               // 通知主线程，登录结果处理完成
            continue;
        }
        // 注册响应消息
        if (REG_ACK == msgtype)
        {
            fixbug::RegisterResponse registerresponse;
            registerresponse.ParseFromString(buf);
            doRegisterResponse(registerresponse);
            sem_post(&rwsem); // 通知主线程，注册结果处理完成
            continue;
        }
        // 添加好友响应消息
        if (ADD_FRIEND_ACK == msgtype)
        {
            fixbug::AddFriendResponse addfriendresponse;
            addfriendresponse.ParseFromString(buf);
            if (addfriendresponse.result().success())
            {
                cout << "添加好友成功" << endl;
            }
            else
            {
                cout << "添加好友失败! errmsg:" << addfriendresponse.result().errmsg() << endl;
            }
            continue;
        }
        // 创建群组响应消息
        if (CREATE_GROUP_ACK == msgtype)
        {
            fixbug::CreateGroupResponse creategroupresponse;
            creategroupresponse.ParseFromString(buf);
            if (creategroupresponse.result().success())
            {
                cout << "创建群组成功,群号是:" << creategroupresponse.groupid() << endl;
            }
            else
            {
                cout << "创建群组失败! errmsg:" << creategroupresponse.result().errmsg() << endl;
            }
            continue;
        }
        // 加入群组响应消息
        if (ADD_GROUP_ACK == msgtype)
        {
            fixbug::AddGroupResponse addgroupresponse;
            addgroupresponse.ParseFromString(buf);
            if (addgroupresponse.result().success())
            {
                cout << "加入群组成功" << endl;
            }
            else
            {
                cout << "加入群组失败! errmsg:" << addgroupresponse.result().errmsg() << endl;
            }
            continue;
        }
        // 获取用户好友信息响应消息
        if (GET_FRIENDLIST_ACK == msgtype)
        {
            fixbug::GetFriendListResponse getfriendlistresponse;
            getfriendlistresponse.ParseFromString(buf);
            if (getfriendlistresponse.result().success())
            {
                cout << "获取用户好友列表成功!" << endl;
                g_currentUserFriendList.clear();
                int friend_size = getfriendlistresponse.friendlist().friend__size();
                for (int i = 0; i < friend_size; ++i)
                {
                    User user;
                    user.setUserId(getfriendlistresponse.friendlist().friend_(i).userid());
                    user.setUserName(getfriendlistresponse.friendlist().friend_(i).username());
                    user.setUserState(getfriendlistresponse.friendlist().friend_(i).userstate());
                    g_currentUserFriendList.push_back(user);
                }
            }
            else
            {
                cout << "获取用户好友列表失败! errmsg:" << getfriendlistresponse.result().errmsg() << endl;
            }
            sem_post(&rwsem);
            continue;
        }
        // 获取用户群组信息响应消息
        if (GET_GROUPLIST_ACK == msgtype)
        {
            fixbug::GetGroupListResponse getgrouplistresponse;
            getgrouplistresponse.ParseFromString(buf);
            if (getgrouplistresponse.result().success())
            {
                cout << "获取用户群组列表成功!" << endl;
                g_currentUserGroupList.clear();
                int group_size = getgrouplistresponse.grouplist().group_size();

                for (int i = 0; i < group_size; ++i)
                {
                    Group group;
                    group.setGroupId(getgrouplistresponse.grouplist().group(i).groupid());
                    group.setGroupName(getgrouplistresponse.grouplist().group(i).groupname());
                    group.setGroupDesc(getgrouplistresponse.grouplist().group(i).groupdesc());

                    int group_user_size = getgrouplistresponse.grouplist().group(i).groupmember_size();

                    for (int j = 0; j < group_user_size; ++j)
                    {
                        User user;
                        user.setUserId(getgrouplistresponse.grouplist().group(i).groupmember(j).userid());
                        cout << getgrouplistresponse.grouplist().group(i).groupmember(j).userid() << endl;
                        user.setUserName(getgrouplistresponse.grouplist().group(i).groupmember(j).username());
                        cout << getgrouplistresponse.grouplist().group(i).groupmember(j).username() << endl;
                        user.setUserState(getgrouplistresponse.grouplist().group(i).groupmember(j).userstate());
                        cout << getgrouplistresponse.grouplist().group(i).groupmember(j).userstate() << endl;
                        cout << getgrouplistresponse.grouplist().group(i).memberidentity(j) << endl;
                        group.getGroupUsers().emplace(user, getgrouplistresponse.grouplist().group(i).memberidentity(j));
                    }

                    g_currentUserGroupList.push_back(group);
                }
            }
            else
            {
                cout << "获取用户群组列表失败! errmsg:" << getgrouplistresponse.result().errmsg() << endl;
            }
            sem_post(&rwsem);
            continue;
        }
        // 聊天消息
        if (ONE_CHAT_ACK == msgtype)
        {
            fixbug::OneChatResponse onechatresponse;
            onechatresponse.ParseFromString(buf);
            if (onechatresponse.result().success())
            {
                cout << "发送好友消息成功" << endl;
            }
            else
            {
                cout << "发送好友消息失败! errmsg:" << onechatresponse.result().errmsg() << endl;
            }
            continue;
        }
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << "收到好友消息" << endl;
            fixbug::ChatMessage chatmessage;
            chatmessage.ParseFromString(buf);
            cout << chatmessage.chattime() << "[" << chatmessage.userid() << "]" << chatmessage.username() << ":" << chatmessage.msg() << endl;
            continue;
        }
        // 群聊天消息
        if (GROUP_CHAT_ACK == msgtype)
        {
            fixbug::GroupChatResponse groupchatresponse;
            groupchatresponse.ParseFromString(buf);
            if (groupchatresponse.result().success())
            {
                cout << "发送群组消息成功" << endl;
            }
            else
            {
                cout << "发送群组消息失败! errmsg:" << groupchatresponse.result().errmsg() << endl;
            }
            continue;
        }
        // 群聊天消息
        if (GROUP_CHAT_MSG == msgtype)
        {
            fixbug::ChatMessage chatmessage;
            chatmessage.ParseFromString(buf);
            cout << "群消息:" << chatmessage.chattime() << "[" << chatmessage.groupid() << "]" << chatmessage.groupname() << ":"
                 << "[" << chatmessage.userid() << "]" << chatmessage.username() << ":" << chatmessage.msg() << endl;
            continue;
        }
        // 获取用户离线信息响应消息
        if (GET_OFFLINEMSG_ACK == msgtype)
        {
            fixbug::GetOfflineMsgResponse getofflinemsgresponse;
            getofflinemsgresponse.ParseFromString(buf);
            if (getofflinemsgresponse.result().success())
            {
                cout << "获取离线消息成功" << endl;
                // 显示当前用户的离线消息 个人聊天信息或者群组消息
                int offlinemsg_size = getofflinemsgresponse.offlinemsglist_size();
                for (int i = 0; i < offlinemsg_size; ++i)
                {
                    if (ONE_CHAT_MSG == getofflinemsgresponse.offlinemsglist(i).msgtype())
                    {
                        cout << getofflinemsgresponse.offlinemsglist(i).chattime() << "[" << getofflinemsgresponse.offlinemsglist(i).userid() << "]" << getofflinemsgresponse.offlinemsglist(i).username() << ":" << getofflinemsgresponse.offlinemsglist(i).msg() << endl;
                    }
                    else
                    {
                        cout << "群消息:" << getofflinemsgresponse.offlinemsglist(i).chattime() << "[" << getofflinemsgresponse.offlinemsglist(i).groupid() << "]" << getofflinemsgresponse.offlinemsglist(i).groupname() << ":"
                             << "[" << getofflinemsgresponse.offlinemsglist(i).userid() << "]" << getofflinemsgresponse.offlinemsglist(i).username() << ":" << getofflinemsgresponse.offlinemsglist(i).msg() << endl;
                    }
                }
            }
            else
            {
                cout << "获取离线消息失败! errmsg:" << getofflinemsgresponse.result().errmsg() << endl;
            }
            sem_post(&rwsem);
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "======================login user======================" << endl;
    cout << "current login user => userid:" << g_currentUser.getUserId() << " username:" << g_currentUser.getUserName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getUserId() << " " << user.getUserName() << " " << user.getUserState() << endl;
        }
    }
    cout << "----------------------group list----------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getGroupId() << " " << group.getGroupName() << " " << group.getGroupDesc() << endl;
            for (auto &pairUser : group.getGroupUsers())
            {
                cout << pairUser.first.getUserId() << " " << pairUser.first.getUserName() << " " << pairUser.first.getUserState() << endl;
            }
        }
    }
    cout << "======================================================" << endl;
}

// "help" command handler
void help(int fd = 0, std::string str = "");
// "chat" command handler
void chat(int, std::string);
// "addfriend" command handler
void addfriend(int, std::string);
// "creategroup" command handler
void creategroup(int, std::string);
// "addgroup" command handler
void addgroup(int, std::string);
// "groupchat" command handler
void groupchat(int, std::string);
// "loginout" command handler
void loginout(int, std::string);

// 系统支持的客户端命令列表
unordered_map<std::string, std::string> commandMap = {
    {"help", "显示所有支持的命令，格式help"},
    {"chat", "一对一聊天，格式chat:friendid:message"},
    {"addfriend", "添加好友，格式addfriend:friendid"},
    {"creategroup", "创建群组，格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组，格式addgroup:groupid"},
    {"groupchat", "群聊，格式groupchat:groupid:message"},
    {"loginout", "注销，格式loginout"}};

// 注册系统支持的客户端命令处理
unordered_map<std::string, function<void(int, std::string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        std::string commandbuf(buffer);
        std::string command; // 存储命令
        int idx = commandbuf.find(":");
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else
        {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }

        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
    }
}

// "help" command handler
void help(int, std::string)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}
// "addfriend" command handler
void addfriend(int clientfd, std::string friendid)
{

    fixbug::AddFriendRequest addfriendrequest;
    addfriendrequest.set_userid(g_currentUser.getUserId());
    addfriendrequest.set_friendid(friendid);

    fixbug::protobuffer buffer;
    buffer.set_protobuftype(ADD_FRIEND);
    buffer.set_protobufstr(addfriendrequest.SerializeAsString());

    string request = buffer.SerializeAsString();

    // 发送给登录服务器
    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addfriend msg error -> " << request << endl;
    }
}
// "chat" command handler
void chat(int clientfd, std::string str)
{
    int idx = str.find(":"); // friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }

    std::string friendid = str.substr(0, idx);
    std::string message = str.substr(idx + 1, str.size() - idx);

    fixbug::OneChatRequest onechatrequest;
    fixbug::ChatMessage *chatmessage = onechatrequest.mutable_chatmsg();
    chatmessage->set_chattime(getCurrentTime());
    chatmessage->set_userid(g_currentUser.getUserId());
    chatmessage->set_username(g_currentUser.getUserName());
    chatmessage->set_msg(message);
    chatmessage->set_msgtype(ONE_CHAT_MSG);
    onechatrequest.set_userid(g_currentUser.getUserId());
    onechatrequest.set_friendid(friendid);

    fixbug::protobuffer buffer;
    buffer.set_protobuftype(ONE_CHAT);
    buffer.set_protobufstr(onechatrequest.SerializeAsString());
    string request = buffer.SerializeAsString();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error -> " << request << endl;
    }
}
// "creategroup" command handler  groupname:groupdesc
void creategroup(int clientfd, std::string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }

    std::string groupname = str.substr(0, idx);
    std::string groupdesc = str.substr(idx + 1, str.size() - idx);

    fixbug::CreateGroupRequest creategrouprequest;
    creategrouprequest.set_userid(g_currentUser.getUserId());
    creategrouprequest.set_groupname(groupname);
    creategrouprequest.set_groupdesc(groupdesc);

    fixbug::protobuffer buffer;
    buffer.set_protobuftype(CREATE_GROUP);
    buffer.set_protobufstr(creategrouprequest.SerializeAsString());
    string request = buffer.SerializeAsString();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send creategroup msg error -> " << request << endl;
    }
}
// "addgroup" command handler
void addgroup(int clientfd, std::string groupid)
{

    fixbug::AddGroupRequest addgrouprequest;
    addgrouprequest.set_userid(g_currentUser.getUserId());
    addgrouprequest.set_groupid(groupid);

    fixbug::protobuffer buffer;
    buffer.set_protobuftype(ADD_GROUP);
    buffer.set_protobufstr(addgrouprequest.SerializeAsString());
    string request = buffer.SerializeAsString();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error -> " << request << endl;
    }
}
// "groupchat" command handler   groupid:message
void groupchat(int clientfd, std::string str)
{
    int idx = str.find(":");
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }

    std::string groupid = str.substr(0, idx);
    std::string message = str.substr(idx + 1, str.size() - idx);

    fixbug::GroupChatRequest groupchatrequest;
    fixbug::ChatMessage *chatmessage = groupchatrequest.mutable_chatmsg();
    chatmessage->set_chattime(getCurrentTime());
    chatmessage->set_userid(g_currentUser.getUserId());
    chatmessage->set_username(g_currentUser.getUserName());
    chatmessage->set_groupid(groupid);
    chatmessage->set_groupname(groupid);
    chatmessage->set_msg(message);
    chatmessage->set_msgtype(GROUP_CHAT_MSG);
    groupchatrequest.set_userid(g_currentUser.getUserId());
    groupchatrequest.set_groupid(groupid);

    fixbug::protobuffer buffer;
    buffer.set_protobuftype(GROUP_CHAT);
    buffer.set_protobufstr(groupchatrequest.SerializeAsString());
    string request = buffer.SerializeAsString();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error -> " << request << endl;
    }
}
// "loginout" command handler
void loginout(int clientfd, std::string)
{

    fixbug::LoginoutRequest loginoutrequest;
    loginoutrequest.set_userid(g_currentUser.getUserId());

    fixbug::protobuffer buffer;
    buffer.set_protobuftype(LOGINOUT);
    buffer.set_protobufstr(loginoutrequest.SerializeAsString());
    string request = buffer.SerializeAsString();

    int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send loginout msg error -> " << request << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

// 获取系统时间（聊天信息需要添加时间信息）
std::string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[512] = {0};
    sprintf(date, "%d-%02d-%02d:%02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}