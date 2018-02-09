var firebase = require("firebase");
var fs = require('fs');
const fetch = require("node-fetch");
var TfIdf = require('node-tfidf');
var tfidf = new TfIdf();

// initialize firebase
var config = {
	apiKey: "AIzaSyBwUZxadjpLICEa7KdgOMleq97Ku5r44Y0",
	authDomain: "xkcdalizer.firebaseapp.com",
	databaseURL: "https://xkcdalizer.firebaseio.com",
	storageBucket: "xkcdalizer.appspot.com",
};
firebase.initializeApp(config);
firebase.database().goOnline();
var wikiRef = firebase.database().ref("wiki");
var comicRef = firebase.database().ref("comics");

// load all docs into tfidf ONLY ONCE
wikiRef.once('value').then(function(snapshot){
	snapshot.forEach(function(data){
		tfidf.addDocument(data.val());
	});
});

function Comparator(a, b) {
	if (a[1] < b[1]) return 1;
	if (a[1] > b[1]) return -1;
	return 0;
}

module.exports = {
	Core: function(query) {
		return new Promise(function(resolve, reject){

			var highestArray = [];

			var highest = 0;
			var highestMeasure = 0;

			// use this string for comic 434
			//airplane following assumed lockpicks illegal actually container churchmouse
			tfidf.tfidfs(query, function(i, measure) {
				highestArray.push([i+1,measure])

				/*if (measure > highestMeasure) {
					highest = i+1;
					highestMeasure = measure;
				}*/
			});

			var lnk = "";
			comicRef.once('value').then(function(snapshot){
				highestArray = highestArray.sort(Comparator).slice(0,10)
				var highestArrayImg = []
				highestArray.forEach(data => {
					highestArrayImg.push(snapshot.val()[data[0]]["img"])
				});

				if(highestArrayImg == null) {
					reject("could not find relevant XKCD");
				} else {
					resolve(highestArrayImg);
				}
			});

		});
	}
}


