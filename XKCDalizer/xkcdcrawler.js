
var firebase = require("firebase");
const fetch = require("node-fetch");

var config = {
    apiKey: "AIzaSyBwUZxadjpLICEa7KdgOMleq97Ku5r44Y0",
    authDomain: "xkcdalizer.firebaseapp.com",
    databaseURL: "https://xkcdalizer.firebaseio.com",
    storageBucket: "xkcdalizer.appspot.com",
  };
firebase.initializeApp(config);
firebase.database().goOnline();
var db = firebase.database();

function getcomic(index) {

    fetch(`https://xkcd.com/${index}/info.0.json`).then(response => {
        response.json().then(json => {
            db.ref(`comics/${json["num"]}`).set({
                year: json["year"],
                month: json["month"],
                day: json["day"],
                title: json["title"],
                safe_title: json["safe_title"],
                transcript: json["transcript"], // TODO needs serious parsing
                alt: json["alt"],
                img: json["img"]
            });
        });
    });
}

for(var i = 1; i < 1908; i++) {
    getcomic(i);
}
console.log("DONE");

function pushComic(id, text) {
    firebase.database().ref(`wiki/${id}`).set(text);
}

function getExplained(number) {
    var output = ''
    request('http://www.explainxkcd.com/wiki/index.php/'+number, function (error, response, body) {
        //console.log('error:', error); // Print the error if one occurred
        //console.log('statusCode:', response && response.statusCode); // Print the response status code if a response was received
        const $ = cheerio.load(body);

        for (var i = 0; i < $('#mw-content-text').contents('p').length; i++) {
            if ($('#mw-content-text').contents('p')[i].name == 'p') {
                //console.log($('#mw-content-text').contents('p')[i])
                for (var j = 0; j < $('#mw-content-text').contents('p')[i].children.length; j++) {
                    if ($('#mw-content-text').contents('p')[i].children[j].data != undefined) {
                        output += ($('#mw-content-text').contents('p')[i].children[j].data)
                    }
                    else {
                        for (var k = 0; k < $('#mw-content-text').contents('p')[i].children[j].children.length; k++) {
                           //console.log($('#mw-content-text').contents('p')[i].children[j].children[k].data)
                        }
                        
                    }
                }
            }
        }
        pushComic(number, output);
        //console.log(output);
    });
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/*
for (var i = 1; i <=1912 ; i++) {
    getExplained(i);
}*/