#include <set>
#include <map>

#include "../http_server.h"
#include "../util.h"
#include "../http_client.h"
#include "../lock_guard.h"

using namespace mevent;
using namespace mevent::util;


const char *index_html =
"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n"
"    \"http://www.w3.org/TR/html4/strict.dtd\">\n"
"<html>\n"
"<head>\n"
"    <meta http-equiv=\"Content-type\" content=\"text/html charset=utf-8\" />\n"
"    <title>mevent - chat room</title>\n"
"</head>\n"
"<body>\n"
"    <p>\n"
"        <span style=\"display:block; float:left\">Chat:</span>\n"
"        <textarea name=\"chat\" rows=\"8\" cols=\"40\" id=\"chat\" readonly=\"readonly\"></textarea>\n"
"    </p>\n"
"    <p>\n"
"        <span style=\"display:block; float:left\">Status:</span>\n"
"        <span class=\"online\" style=\"display:none; color:green\">online</span>\n"
"        <span class=\"offline\" style=\"display:block; color:red\">offline</span>\n"
"    </p>\n"
"    <p>\n"
"        <label for=\"room\">Room:<span>\n"
"        <input type=\"text\" name=\"room\" value=\"10086\" id=\"room\" />\n"
"    </p>\n"
"    <p>\n"
"        <span for=\"nick\">Nick:<span>\n"
"        <input type=\"text\" name=\"nick\" value=\"anonymous\" id=\"nick\" />\n"
"    </p>\n"
"    <p>\n"
"        <span for=\"message\">Message:<span>\n"
"        <input type=\"text\" name=\"message\" value=\"\" id=\"message\" />\n"
"    </p>\n"
"    <p>\n"
"        <input type=\"button\" value=\"Send\" id=\"sendButton\" />\n"
"        <input type=\"button\" value=\"Clear\" id=\"clearButton\" />\n"
"    </p>\n"
"<script src=\"https://apps.bdimg.com/libs/jquery/2.1.4/jquery.min.js\" type=\"text/javascript\" language=\"javascript\" charset=\"utf-8\"></script>\n"
"<script type=\"text/javascript\">\n"
"var ws\n"
"var room = \"default\"\n"
"\n"
"function init_ws() {\n"
"    if ($(\"#room\").val() != room) {\n"
"        room = $(\"#room\").val()\n"
"    } else {\n"
"        return\n"
"    }\n"
"\n"
"    url = \"ws://\" + window.location.hostname + \":\" + window.location.port + \"/ws?room=\" + room\n"
"    if (location.protocol == \"https:\") \n"
"        url = \"wss://\" + window.location.hostname + \":\" + window.location.port + \"/ws?room=\" + room\n"
"    ws = new WebSocket(url)\n"
"    ws.onopen = function(event) {\n"
"        $(\".offline\").hide()\n"
"        $(\".online\").show()\n"
"    }\n"
"\n"
"    ws.onclose = function(event) {\n"
"        $(\".offline\").show()\n"
"        $(\".online\").hide()\n"
"    }\n"
"\n"
"    ws.onmessage = function(event) {\n"
"        if (event.data == \"\") return\n"
"        var txt = $(\"textarea#chat\")\n"
"        txt.val(txt.val() + event.data + \"\\n\")\n"
"        txt.scrollTop(txt[0].scrollHeight - txt.height())\n"
"    }\n"
"}\n"
"\n"
"function send() {\n"
"    var nick = \"anonymous\"\n"
"    if ($(\"#nick\").val() != \"\") {\n"
"        nick = $(\"#nick\").val()\n"
"    }\n"
"    \n"
"    ws.send(nick + \":\" + ($(\"#message\").val()))\n"
"    $(\"#message\").val(\"\")\n"
"}\n"
"\n"
"$(\"#sendButton\").click(function() {\n"
"    send()\n"
"    return false\n"
"})\n"
"\n"
"$(\"#message\").on(\"keyup\", function(event) {\n"
"    if (event.keyCode == 13) {\n"
"        send()\n"
"    }\n"
"})\n"
"\n"
"$(\"#clearButton\").click(function() {\n"
"    var txt = $(\"textarea#chat\")\n"
"    txt.val(\"\")\n"
"})\n"
"\n"
"$(\"#room\").focusout(function() {\n"
"    ws.onclose = function() {}\n"
"    ws.onmessage = function() {}\n"
"    ws.onopen = function() {}\n"
"    ws.close()\n"
"    init_ws()\n"
"})\n"
"\n"
"init_ws()\n"
"setInterval(function(){ ws.send(\"\") }, 5000)//heartbeat\n"
"\n"
"</script>\n"
"</body>\n"
"</html>\n";

class ChatRoom {
public:
    struct Task {
        std::string channel_name;
        std::vector<uint8_t> data;
    };
    
    ChatRoom () {
        pthread_mutex_init(&task_mtx_, NULL);
        pthread_cond_init(&task_cond_, NULL);
        
        pthread_mutex_init(&clients_mtx_, NULL);
        
        pthread_t tid;
        pthread_create(&tid, NULL, TaskThread, this);
        pthread_detach(tid);
    }
    
    void Index(Connection *conn) {
        conn->Resp()->SetHeader("Content-Type", "text/html");
        conn->Resp()->WriteString(std::string(index_html));
    }
    
    void OnClose(WebSocket *ws) {
        LockGuard lock_guard(clients_mtx_);
        
        auto it = channel_index_map_.find(ws);
        if (it == channel_index_map_.end()) {
            return;
        }
        
        auto client_it = clients_map_.find(it->second);
        if (client_it == clients_map_.end()) {
            return;
        }
        
        client_it->second.erase(ws);
        if (client_it->second.empty()) {
            clients_map_.erase(client_it);
        }
    }
    
    void OnMessage(WebSocket *ws, const std::string &msg) {
        if (msg.empty()) {
            ws->WriteString(msg);
        } else {
            TaskPush(ws, msg);
        }
    }
    
    void Ping(WebSocket *ws, const std::string &msg) {
        (void)msg;//avoid unused parameter warning
        ws->SendPong("");
    }
    
    void Subscribe(Connection *conn) {
        conn->Req()->ParseQueryString();
        std::string channel_name = conn->Req()->QueryStringValue("room");
        if (channel_name.empty()) {
            channel_name = "default";
        }
        
        if (!conn->WS()->Upgrade()) {
            conn->Resp()->WriteErrorMessage(500);
            return;
        }
        
        conn->WS()->SetPingHandler(std::bind(&::ChatRoom::Ping, this, std::placeholders::_1, std::placeholders::_2));
        conn->WS()->SetOnCloseHandler(std::bind(&::ChatRoom::OnClose, this, std::placeholders::_1));
        conn->WS()->SetOnMessageHandler(std::bind(&::ChatRoom::OnMessage, this, std::placeholders::_1, std::placeholders::_2));
        
        conn->WS()->SetMaxBufferSize(100000);
        
        {
            LockGuard lock_guard(clients_mtx_);

            channel_index_map_[conn->WS()] = channel_name;
            
            auto it = clients_map_.find(channel_name);
            if (it == clients_map_.end()) {
                std::set<WebSocket *> clients;
                clients.insert(conn->WS());
                clients_map_[channel_name] = clients;
            } else {
                it->second.insert(conn->WS());
            }
        }
    }
    
    void TaskPush(WebSocket *ws, const std::string &msg) {
        LockGuard lock_guard(task_mtx_);
        
        auto it = channel_index_map_.find(ws);
        if (it == channel_index_map_.end()) {
            return;
        }
        
        std::vector<uint8_t> data;
        ws->MakeFrame(data, msg, WebSocketOpcodeType::TEXT_FRAME);
        
        std::shared_ptr<Task> task_ptr = std::make_shared<Task>();
        
        task_ptr->data = data;
        task_ptr->channel_name = it->second;
        task_que_.push(task_ptr);
        
        pthread_cond_signal(&task_cond_);
    }
    
    static void *TaskThread(void *arg) {
        ChatRoom *chat = static_cast<ChatRoom *>(arg);
        while (true) {
            std::shared_ptr<Task> task_ptr;
            {
                LockGuard lock_guard(chat->task_mtx_);
                if (chat->task_que_.empty()) {
                    pthread_cond_wait(&chat->task_cond_, &chat->task_mtx_);
                    continue;
                }
                task_ptr = chat->task_que_.front();
                chat->task_que_.pop();
            }
            
            std::set<WebSocket *> clients;
            {
                LockGuard lock_guard(chat->clients_mtx_);

                auto it = chat->clients_map_.find(task_ptr->channel_name);
                if (it == chat->clients_map_.end()) {
                    continue;
                }
                
                clients = it->second;
            }

            auto it = clients.begin();
            for (; it != clients.end(); it++) {
                (*it)->WriteRawDataSafe(task_ptr->data);
            }
        }
        
        return (void *)0;
    }
    
    pthread_mutex_t clients_mtx_;
    
    pthread_cond_t task_cond_;
    pthread_mutex_t task_mtx_;
    std::queue<std::shared_ptr<Task>> task_que_;
    
    std::map<WebSocket *, std::string> channel_index_map_;
    std::map<std::string, std::set<WebSocket *>> clients_map_;
};

int main() {
    ChatRoom chat;
    
    HTTPServer *server = new HTTPServer();
    server->SetHandler("/", std::bind(&::ChatRoom::Index, &chat, std::placeholders::_1));
    server->SetHandler("/ws", std::bind(&::ChatRoom::Subscribe, &chat, std::placeholders::_1));
    
    server->SetWorkerThreads(4);
    server->SetIdleTimeout(60);
    server->SetMaxWorkerConnections(8192);
    
    server->ListenAndServe("0.0.0.0", 80);
    
    //HTTPS WSS
//    server->ListenAndServeTLS("0.0.0.0", 443, "host.crt", "host.key");
    
    return 0;
}
