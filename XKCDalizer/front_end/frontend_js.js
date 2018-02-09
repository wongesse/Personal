
// global vars
var resultJson = null; // holds the resulting json
var galleryIndex = null; // where we are in the gallery

function prepQuery() {
    var value = $("#dropSelect").val();
    // for plaintext
    if(value == 1) {
        textQuery();
    }
    // for hash
    else if(value == 2) {
        hashQuery();
    }
    else {
        // error
    }
}

function hashQuery() {
    var xmlhttp = new XMLHttpRequest()
    queryField = encodeURIComponent(document.getElementById('inputField').value);

    showElement("#twitterImage");
    document.getElementById("twitterImage").href='https://twitter.com/search?q=%23'+encodeURIComponent(queryField)+'&src=typd'
    document.getElementById("twitterImage").innerHTML='#'+queryField+' Tweets'
    
    xmlhttp.onreadystatechange = function () {
        if (this.readyState == 4) {
            
            resultJson = JSON.parse(xmlhttp.response);
            if (resultJson["imgs"].length == 0) {
                // handle case where no images returned
            }
            //document.getElementById("twitterWidget").href = "https://twitter.com/hashtag/" + queryField
            galleryIndex = -1;   
            $("#nextClick").click();

        }
    }
    xmlhttp.open("GET", '/' + 'hash/' + queryField, true)
    xmlhttp.send()
}


function textQuery() {
    var xmlhttp = new XMLHttpRequest()
    var query = encodeURIComponent(document.getElementById('inputField').value);

    hideElement("#twitterImage")

    xmlhttp.onreadystatechange = function () {

        if (this.readyState == 4) {
            
            resultJson = JSON.parse(xmlhttp.response);
            if (resultJson["imgs"].length == 0) {
                // handle case where no images returned
                alert("no results found!");
            }
            //document.getElementById("twitterWidget").href = "https://twitter.com/hashtag/" + queryField
            galleryIndex = -1;
            $("#nextClick").click();

        }
    }

    xmlhttp.open("GET", '/' + 'query/' + query, true);
    xmlhttp.send();
}

function getMeta(url) {
    var img = new Image();
    img.onload = function () {
        tempHeight = this.height / 2
        document.getElementById("backButton").style.paddingTop = tempHeight + "px"
        document.getElementById("forwardButton").style.paddingTop = tempHeight + "px"
    };
    img.src = url;
}

function hideElement(selector) {
    $(selector).css('visibility', 'hidden');
}
function showElement(selector) {
    $(selector).css('visibility', 'visible');
}

function handleImageChange() {
    if(galleryIndex == 0) {
        hideElement("#backClick");
    }
    else if(galleryIndex > 0) {
        showElement("#backClick");
    }
    else {
        // illegal, galleryIndex < 0
    }

    if(galleryIndex == resultJson["imgs"].length - 1) {
        hideElement("#nextClick");
    }
    else if(galleryIndex < resultJson["imgs"].length -1) {
        showElement("#nextClick");
    }
    else {
        // illegal, galleryIndex beyond images
    }

    // now we move the image
    $("#resultImage").attr("src", resultJson["imgs"][galleryIndex]);
}

$(document).ready(function () {

    // listeners
    $('#nextClick').click(function () {
        galleryIndex += 1;
        handleImageChange();
        getMeta(resultJson["imgs"][galleryIndex])
        
    });
    $('#backClick').click(function () {
        galleryIndex -= 1;
        handleImageChange();
        getMeta(resultJson["imgs"][galleryIndex])
    });
    // material
    $('select').material_select();
    //$(".dropdown-content>li>a").css("color", themeColor);
});


