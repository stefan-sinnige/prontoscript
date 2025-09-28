/*
 * Simple JSON handling
 */

System.include("com.philips.HttpLibrary.js");
System.include("json2.js");

function verifyReply(aData) {
    /* Examine the JSON data */
    json = JSON.parse(aData);
    if (json[0]["artist"] == "Fleetwood Mac") {
        System.print("PASS: querying artist from first entry");
    }
    else {
        System.print("FAIL: expected \"Fleetwood Mac\", " +
                     "but received \"" + json[0]["artist"] +"\"");
    }
    if (json[1]["title"] == "Communication Breakdown") {
        System.print("PASS: querying title from second entry");
    }
    else {
        System.print("FAIL: expected \"Communication Breakdown\", " +
                     "but received \"" + json[1]["title"] +"\"");
    }
}

var httpLib = com.philips.HttpLibrary;
httpLib.getHTTP("http://localhost:52001/json-1", verifyReply);
