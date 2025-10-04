/*
 * Simple JSON handling
 */

System.include("com.philips.HttpLibrary.js");
System.include("json2.js");

function listDictionaryTest() {
    var httpLib = com.philips.HttpLibrary;
    var handled = false;
    httpLib.getHTTP("http://localhost:52001/json-1", function(aData){
        handled = true;
        json = JSON.parse(aData);
        suite.assert("Fleetwood Mac", json[0]["artist"]);
        suite.assert("Communication Breakdown", json[1]["title"]);
    });
    suite.events();
    suite.assert(handled, true);
}

var suite = new JSUnit("JSON Lists over HTTP");
suite.add("Access entries in a list of dictionaries", listDictionaryTest);
suite.run();
