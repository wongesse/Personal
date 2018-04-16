var Twit = require('twit');

var T = new Twit({
    consumer_key:         'mjHwqoReJn1BGJbxJZ3wt7u0x'
  , consumer_secret:      'N2RV7aiwLXzBLyD1mJnFIeA8K0Qqe9xtzzqR6oNE4FDnoqkXeC'
  , access_token:         '864774372-Y29O11hMblU8cgjZK5J3vlVlSkhh9ZhQNsFXoNxM'
  , access_token_secret:  'vfX0horPzXaRMBML7qUfSp9e99julSK5WpzmY7tHw5mXt'
})

var tweet = ""
module.exports = {
	Core: function(query) {
		return new Promise(function(resolve, reject){
			var params = {
				q: query,
				count: 10
			}
			var tweet = []
			T.get('search/tweets', params,
				function gotData(err,data,response) {
					var tweets = data.statuses;
					for (var i = 0 ; i < tweets.length; i++){
						tweet.push(tweets[i].text);
					}
					resolve(tweet);
				}
			);

		});
	}
}
