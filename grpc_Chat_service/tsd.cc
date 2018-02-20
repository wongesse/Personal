#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>
#include <thread>
#include <unordered_map>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <fstream>
#include <sstream>

#include <grpc++/grpc++.h>

#include "tinysocialnetwork.grpc.pb.h"
#include "json.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using tinysocialnetwork::CreateRequest;
using tinysocialnetwork::CreateReply;
using tinysocialnetwork::UserRequest;
using tinysocialnetwork::UserReply;
using tinysocialnetwork::ListRequest;
using tinysocialnetwork::ListReply;
using tinysocialnetwork::TimelineStream;
using tinysocialnetwork::SNetwork;

using namespace std;
using json = nlohmann::json;

struct TimelinePost{
    string username;    // User who made post
    string post;        // Post content/message
    string time;        // Time the post was made
};

struct User {
    string username;                // User the object represents
    vector<string> followers;       // List of usernames following the user
    vector<TimelinePost> timeline;  // The list of timeline posts owned by the user
    ServerReaderWriter<TimelineStream, TimelineStream>* t_stream;   // The stream connected to the user in timeline mode
};

// Map is global to allow persistence
map<string, User> users;

class SNetworkServiceImpl final : public SNetwork::Service {

    Status create_user(ServerContext* context, const CreateRequest* request,
            CreateReply* reply) override {

        string username = request->username();

        if (users.count(username) > 0) {
            reply->set_status("FAILURE_ALREADY_EXISTS");
            return Status::OK;
        }

        // Create new user object
        User new_user;
        new_user.username = username;
        new_user.followers = vector<string>();
        new_user.followers.push_back(username);
        new_user.timeline = vector<TimelinePost>();
        new_user.t_stream = nullptr;

        users[username] = new_user;

        reply->set_status("SUCCESS");

        return Status::OK;
    }

    Status follow(ServerContext* context, const UserRequest* request,
            UserReply* reply) override {

        string user_requesting = request->requestuser();
        string user_to_follow = request->targetuser();

        if (user_requesting == user_to_follow) {
            reply->set_status("FAILURE_INVALID_USERNAME");
            return Status::OK;
        }

        // If the username to follow is not in the system
        if (users.count(user_to_follow) < 1) {
            reply->set_status("FAILURE_INVALID_USERNAME");
            return Status::OK;
        }

        // If the source user is already following the user
        if(find(users[user_to_follow].followers.begin(), users[user_to_follow].followers.end(), user_requesting) != users[user_to_follow].followers.end()) {
            reply->set_status("FAILURE_ALREADY_EXISTS");
            return Status::OK;
        }

        users[user_to_follow].followers.push_back(user_requesting);
        reply->set_status("SUCCESS");

        return Status::OK;
    }

    Status unfollow(ServerContext* context, const UserRequest* request,
            UserReply* reply) override {
        
        string user_requesting = request->requestuser();
        string targetuser = request->targetuser();

        if (user_requesting == targetuser) {
            reply->set_status("FAILURE_INVALID_USERNAME");
            return Status::OK;
        }

        // If the username to follow is not in the system
        if (users.count(targetuser) < 1) {
            reply->set_status("FAILURE_INVALID_USERNAME");
            return Status::OK;
        }

        // Check to see if the source user is following the user they are trying to unfollow
        if (find(users[targetuser].followers.begin(), users[targetuser].followers.end(), user_requesting) != users[targetuser].followers.end()) {
            users[targetuser].followers.erase(find(users[targetuser].followers.begin(), users[targetuser].followers.end(), user_requesting));
            reply->set_status("SUCCESS");
        } else {
            reply->set_status("FAILURE_INVALID");
        }

        return Status::OK;
    }

    Status list(ServerContext* context, const ListRequest* request,
            ListReply* reply) override {

        // Necessary to see who is following the requesting user.
        string username = request->username();

        for (auto& user : users) {
            reply->add_users(user.first);
        }

        for (auto& follower : users[username].followers) {
            reply->add_followers(follower);
        }

        reply->set_status("SUCCESS");

        return Status::OK;
    }

    Status timeline(ServerContext* context, 
            ServerReaderWriter<TimelineStream, TimelineStream>* stream) override {

        // First, receive the initial message from the client containing
        // the username associated with this stream
        TimelineStream user;
        stream->Read(&user);

        string current_user = user.username();

        // Save this stream pointer to get up to date posts
        users[current_user].t_stream = stream;

        // Send the client the last twenty items in their timeline
        for (int i = 0; i < 20; i++) {
            if (i < users[current_user].timeline.size()) {
                TimelineStream send_obj;
                send_obj.set_username(users[current_user].timeline[i].username);
                send_obj.set_post(users[current_user].timeline[i].post);
                send_obj.set_time(users[current_user].timeline[i].time);
                stream->Write(send_obj);
            }
        }

        TimelineStream t;
        while(stream->Read(&t)){
            string poster_username = t.username();
            string post = t.post();

            // Create a timeline post object from the received stream
            TimelinePost post_obj;
            post_obj.username = poster_username;
            post_obj.post = post;

            auto tm_time = chrono::system_clock::to_time_t (chrono::system_clock::now());;
            auto tm = *localtime(&tm_time);
            stringstream ss;
            ss << put_time(&tm, "%d-%m-%Y %H-%M-%S");
            post_obj.time = ss.str();

            // Broadcast this post to all followers
            for (auto follower : users[poster_username].followers) {
                users[follower].timeline.push_back(post_obj);
                // Check if the follower is online, send it through their stream if so
                if (users[follower].t_stream != nullptr) {
                    users[follower].t_stream->Write(t);
                }
            }
        }

        // Set the stream pointer to nullptr to indicate the user is offline
        users[current_user].t_stream = nullptr;

        return Status::OK;

    }

};

inline bool file_exists (const string& name) {
    return (access (name.c_str(), F_OK) != -1);
}

// Backs the existing map up to the json file
void persist_users(string database_file_name) {
    if (!file_exists(database_file_name)) {
        fstream outfile(database_file_name);
    }
    
    ofstream out(database_file_name);
    json final_list;

    // Go user by user through the map
    for (map<string, User>::iterator it = users.begin(); it!=users.end(); it++) {

        // Create an equivalent user json object from the user structs
        json json_user;
        json_user["username"] = it->second.username;
        
        json followers_vec(it->second.followers);
        json_user["followers"] = followers_vec;

        json timelinepost_vec;
        
        // Do the same for all of the user's timeline posts
        for (int i = 0; i < it->second.timeline.size(); i++) {
            json timeline_post;
            timeline_post["username"] = it->second.timeline[i].username;
            timeline_post["post"] = it->second.timeline[i].post;
            timeline_post["time"] = it->second.timeline[i].time;
            timelinepost_vec.push_back(timeline_post);
        }

        json_user["timeline"] = timelinepost_vec;
        final_list.push_back(json_user);
    }

    out << final_list;
    out.close();
}

// Takes the existing json file and uploads its contents to the map on program startup
void download_users(string database_file_name) {
    if (file_exists(database_file_name)) {
        ifstream i(database_file_name);
        json json_user;
        i >> json_user;

        // Go through every JSON user in the timeline_database.json file
        for (int i = 0; i < json_user.size(); i++) {

            User map_user;
            string username = json_user[i]["username"];
            map_user.username = username;

            // Retrieve all of the JSON user's follower strings
            for (int j = 0; j < json_user[i]["followers"].size(); j++) {
                string follower_string = json_user[i]["followers"][j];
                map_user.followers.push_back(follower_string);
            }

            // Retrieve all of the JSON User's timeline posts
            for (int k = 0; k < json_user[i]["timeline"].size(); k++) {
                TimelinePost timeline;

                string timeline_username = json_user[i]["timeline"][k]["username"];
                string timeline_post = json_user[i]["timeline"][k]["post"];
                string timeline_time = json_user[i]["timeline"][k]["time"];

                timeline.username = timeline_username;
                timeline.post = timeline_post;
                timeline.time = timeline_time;

                map_user.timeline.push_back(timeline);
            }

            // Add user back to map in service
            users[username] = map_user;
            users[username].t_stream = nullptr;
        
        }
    } else {
        fstream outfile(database_file_name);
        outfile.open(database_file_name, fstream::in | fstream::out | fstream::trunc);
        outfile << "[]";
        outfile.close();
    }
}

// SIGINT handler for persisting database on shutdown
void int_handler(int x) {
    persist_users("timeline_database.json");
    exit(1);
}

int main(int argc, char ** argv) {

    string hostname = "localhost";
    string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1) {
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }

    // Install SIGINT handler
    signal(SIGINT, int_handler);
    download_users("timeline_database.json");

    // Build the grpc server
    SNetworkServiceImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(hostname + ":" + port, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    unique_ptr<Server> server(builder.BuildAndStart());

    server->Wait();
}