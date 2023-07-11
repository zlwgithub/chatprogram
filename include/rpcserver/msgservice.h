#pragma once

#include <muduo/net/TcpConnection.h>
#include <unordered_map>
#include <functional>
#include <mutex>

#include "redis.h"
#include "groupmodel.h"
#include "friendmodel.h"
#include "usermodel.h"
#include "offlinemessagemodel.h"
#include "qq.pb.h"
#include "Log.h"
using namespace std;

class MsgService : public fixbug::MsgServiceRpc
{
public:
    // bool OneChat(string friendid, string msg)
    // {
    //     // 查询toid是否在线
    //     LOGINFO("查询%s是否在线!", friendid.c_str());
    //     User user = UserModel::GetInstance().query(friendid);
    //     LOGINFO("查询%s完成,状态为%s!", friendid.c_str(), user.getUserState().c_str());
    //     if (user.getUserState() == "online")
    //     {
    //         LOGINFO("%s在线!使用redis发布订阅", friendid.c_str());
    //         _redis.publish(stoi(friendid), msg);
    //         return true;
    //     }
    //     LOGINFO("%s不在线，存储离线消息!", friendid.c_str());
    //     // toid不在线，存储离线消息
    //     OfflineMsgModel::GetInstance().insert(friendid, msg);

    //     return true;
    // }

    // void OneChat(::google::protobuf::RpcController *controller,
    //              const ::fixbug::OneChatRequest *request,
    //              ::fixbug::OneChatResponse *response,
    //              ::google::protobuf::Closure *done)
    // {
    //     string friendid = request->friendid();
    //     string msg = request->chatmsg().SerializeAsString();
    //     bool ret = OneChat(friendid, msg);

    //     response->mutable_result()->set_success(ret);
    //     response->mutable_result()->set_errmsg("");

    //     done->Run();
    // }

    vector<string> GetOfflineMsg(string userid)
    {
        vector<string> vec = OfflineMsgModel::GetInstance().query(userid);
        if (!vec.empty())
        {
            // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
            OfflineMsgModel::GetInstance().remove(userid);
        }
        return vec;
    }

    void GetOfflineMsg(::google::protobuf::RpcController *controller,
                       const ::fixbug::GetOfflineMsgRequest *request,
                       ::fixbug::GetOfflineMsgResponse *response,
                       ::google::protobuf::Closure *done)
    {
        string userid = request->userid();
        vector<string> ret = GetOfflineMsg(userid);

        response->mutable_result()->set_success(true);
        response->mutable_result()->set_errmsg("");
        for (auto msg : ret)
        {
            fixbug::ChatMessage chatmsg;
            chatmsg.ParseFromString(msg);
            *(response->add_offlinemsglist()) = chatmsg;
        }

        done->Run();
    }
};