var express = require('express');
var path = require('path');
var logger = require('morgan');
var tkcd = require('./CoreAlgo.js');
var twitter = require('./bot.js');

var app = express();

// uncomment after placing your favicon in /public
//app.use(favicon(path.join(__dirname, 'front_end', 'favicon.ico')));
app.use(logger('dev'))
app.use(express.static(path.join(__dirname, 'front_end')));

// get request for main page
app.get('/', function (request, response) {
    response.sendFile('index.html', { root: path.join(__dirname, './front_end') });
});

app.get('/query/:text', function(request, response) {
    var dat = decodeURIComponent(request.params["text"]);
    console.log(`received ${dat}`);
    var json = {
        "tweets": [],
        "imgs":[]
    };
    tkcd.Core(dat).then(comics => {
        json["imgs"] = comics;
        response.json(json);
    });
});

app.get('/hash/:tag', function(request, response) {
    var hashtag = decodeURIComponent(request.params["tag"]);
    var json = {
        "tweets": [],
        "imgs":[]
    };
    twitter.Core(hashtag).then(tweets => {
        json["tweets"] = tweets;
        // convert to single string
        var tweetStr = ""
        tweets.forEach(element => {
            tweetStr += element;
        });
        tweetStr = tweetStr.replace(/\W/g, ' ');

        tkcd.Core(tweetStr).then(comics => {
            json["imgs"] = comics;
            response.json(json);
        })
    });
});

app.listen(process.env.PORT || 3000, function () {
    console.log('listening on port 3000');
});
