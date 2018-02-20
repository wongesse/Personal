#include <iostream>
#include <thread>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "tinysocialnetwork.grpc.pb.h"
#include "client.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
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

class Client : public IClient
{
    public:
        Client(const string& hname,
               const string& uname,
               const string& p)
            :hostname(hname), username(uname), port(p)
            {}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(string& input);
        virtual void processTimeline();
    private:
        string hostname;
        string username;
        string port;
        
        // You can have an instance of the client stub
        // as a member variable.
        unique_ptr<SNetwork::Stub> stub_;
};

int main(int argc, char** argv) {

    string hostname = "localhost";
    string username = "default";
    string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	auto channel = CreateChannel(this->hostname + ":" + this->port, grpc::InsecureChannelCredentials());
    this->stub_ = tinysocialnetwork::SNetwork::NewStub(channel);

    // Attempt to create a new user. If it already exists thats fine,
    // this just needs to confirm that the server has this user listed
    ClientContext context;
    CreateRequest request;
    CreateReply reply;

    request.set_username(this->username);

    Status status = this->stub_->create_user(&context, request, &reply);

    // If the status in the reply object is either successful or the username already exists, then
    // the connection was successful
    if (status.ok()) {
        string create_status = reply.status();
        if (create_status == "SUCCESS" || create_status == "FAILURE_ALREADY_EXISTS") {
            return 1;
        } else {
            return -1;
        }
    } else {
        return -1; // There was an issue with grpc
    }
}

IReply Client::processCommand(string& input)
{
    // Grab the command from the input string
    string command = input.substr(0, input.find(" "));
    string uname = "";

    // Change the command to all caps
    for (int i = 0; i < command.length(); i++) {
        command[i] = toupper(command[i]);
    }

    // These variables are standard grpc variables that will be reused.
    ClientContext context;
    Status status;

    // IReply struct to be populated with information from replies
    IReply ire;
    string server_status = "";
    
    if (command == "FOLLOW" || command == "UNFOLLOW") {

        // Pull username from input string
        uname = input.substr(input.find(" ") + 1, input.length());

        // Now set up the request and reply objects with given information
        UserRequest request;
        UserReply reply;

        request.set_requestuser(this->username);
        request.set_targetuser(uname);

        if (command == "FOLLOW") {
            status = this->stub_->follow(&context, request, &reply);
        } else {
            status = this->stub_->unfollow(&context, request, &reply);
        }

        ire.grpc_status = status;

        server_status = reply.status();

    } else if (command == "LIST") {

        // Set up request and reply with necessary information
        ListRequest request;
        ListReply reply;

        request.set_username(this->username);

        status = this->stub_->list(&context, request, &reply);

        // Place all user strings in ire structure
        for (auto user : reply.users()) {
            ire.all_users.push_back(user);
        }

        // Place all follower strings in ire structure
        for (auto follower : reply.followers()) {
            ire.followers.push_back(follower);
        }

        ire.grpc_status = status;

        server_status = reply.status();

    } else if (command == "TIMELINE") {
        processTimeline();
        return ire;
    }

    // Determine which enum to use for the status value
    if (server_status == "SUCCESS") {
        ire.comm_status = SUCCESS;
    } else if (server_status == "FAILURE_ALREADY_EXISTS") {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
    } else if (server_status == "FAILURE_NOT_EXISTS") {
        ire.comm_status = FAILURE_NOT_EXISTS;
    } else if (server_status == "FAILURE_INVALID_USERNAME") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (server_status == "FAILURE_INVALID") {
        ire.comm_status = FAILURE_INVALID;
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }
    
    return ire;
}

void Client::processTimeline()
{
    string uname = this->username;

    // Create the bidirectional stream
    ClientContext context;
    std::shared_ptr<ClientReaderWriter<TimelineStream, TimelineStream>> stream(
            stub_->timeline(&context));

    // Send the username initially so the server knows who is using this stream.
    TimelineStream t;
    t.set_username(this->username);
    stream->Write(t);

    // Thread used to take posts from user input and send them to the server.
    thread writer([uname, stream]() {
            TimelineStream t;
            string msg;
            while (1) {
                msg = getPostMessage();
                t.set_username(uname);
                t.set_post(msg);
                t.set_time("");
                stream->Write(t);
            }
            stream->WritesDone();
    });

    // Thread used to read new posts from the server and display them to the user
    thread reader([stream]() {
            TimelineStream t;
            while(stream->Read(&t)) {
                struct tm tm;
                strptime(t.time().c_str(), "%d-%m-%Y %H-%M-%S", &tm);
                time_t time = mktime(&tm);
                displayPostMessage(t.username(), t.post(), time);
            }
    });

    writer.join();
    reader.join();

}
