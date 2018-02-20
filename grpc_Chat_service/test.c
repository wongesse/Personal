#include <unistd.h>
#include <iostream>
#include <fstream>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;

struct TimelinePost{
    string username;
    string post;
    string time;
};

struct User {
    string username;
    vector<string> followers;
    vector<TimelinePost> timeline;
};

inline bool file_exists (const string& name) {
    return (access (name.c_str(), F_OK) != -1);
}

int main() {
	User test_u;
	test_u.username = "test username";

	vector<string> test_followers;
	test_followers.push_back("test follower");
	test_followers.push_back("test follower1");
	test_followers.push_back("test follower2");

	test_u.followers = test_followers;

	TimelinePost test_timelinepost;
	test_timelinepost.username = "test username1";
	test_timelinepost.post = "test post1";
	test_timelinepost.time = "test time1";

	TimelinePost test_timelinepost_2;
	test_timelinepost_2.username = "test username2";
	test_timelinepost_2.post = "test post2";
	test_timelinepost_2.time = "test time2";

	vector<TimelinePost> test_timeline;
	test_timeline.push_back(test_timelinepost);
	test_timeline.push_back(test_timelinepost_2);

	test_u.timeline = test_timeline;

	if (file_exists("test_persist.json")) {
		// persist data
		cout << "File exists, persisting data" << endl;
	}
	else {
		// create file and persist data
		cout << "File does not exist, creating file" << endl;
		fstream outfile("test_persist.json");
		cout << "Persisting data" << endl;
	}

	ofstream out("test_persist.json");

	json json_user;
	json_user["username"] = test_u.username;
	
	json followers_vec(test_u.followers);
	json_user["followers"] = followers_vec;

	json timelinepost_vec;
	
	for (int i = 0; i < test_u.timeline.size(); i++) {
	    json temp_timeline_post;
	    temp_timeline_post["username"] = test_u.timeline[i].username;
	    temp_timeline_post["post"] = test_u.timeline[i].post;
	    temp_timeline_post["time"] = test_u.timeline[i].time;
	    timelinepost_vec.push_back(temp_timeline_post);
	}

	json_user["timline"] = timelinepost_vec;

	out << setw(4) << json_user << endl;

}