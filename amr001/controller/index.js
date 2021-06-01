const fetch = require('node-fetch');

const[N0, N1, N2, N3, N4, N5, N6, N7, N8, N9, COLON, DOT] = [2122416510, 2156003200, 1636929934, 2173274486, 471006992, 2273937777, 2122942833, 2171148551, 1855033710, 2391904638, 6710784, 12632064];
const CT = 33489153;

function getTimeString() {
    var date = new Date();
    var h = date.getHours();
    h = h < 10 ? '0' + h : h;
    var m = date.getMinutes();
    m = m < 10 ? '0' + m : m;
    var s = date.getSeconds();
    s = s < 10 ? '0' + s : s;
    return `${h}:${m}:${s}`;
}

function parseLinarArray(a) {
    var i = 0;
    a = a.map(e => e === undefined ? 0 : e);
    return [[a[i++], a[i++]], [a[i++], a[i++]], [a[i++], a[i++]], [a[i++], a[i++]]];
};

function remapper(e) {
    if (e == ':') {
        return COLON;
    } else if (e == '.') {
        return DOT;
    } else if (('A' <= e && e <= 'Z') || ('a' <= e && e <= 'z')) {
        return eval('C' + e);
    } else if ('0' <= e && e <= '9') {
        return eval('N' + e);
    } else {
        return 0;
    }
}
function sendText(text) {
    return sendLedControl(text.split('').map(remapper));
}
function sendLedControl(data) {
    return fetch(URL + LED_CONTROL1(parseLinarArray(data)))
    .then(res => res.json()).then(json => {
        console.log("sendLedControl: ", json);
    });
}
function sendLedAnimation(dataArray) {
    return dataArray.reduce(
        (p, e) => p.then(
            () => sendLedControl(e)), Promise.resolve());
}

const URL = "http://192.168.1.20";
const DHT11_1 = `/read?data={"type":"DHT11-1"}`;
const TURN_ON_AIRCON = `/request?data={"type":"IRSend1","func":"sendLG","data":142608649,"nbits":28}`;
const TURN_OFF_AIRCON = `/request?data={"type":"IRSend1","func":"sendLG","data":143392849,"nbits":28}`;
const POWER_MODE_AIRCON = `/request?data={"type":"IRSend1","func":"sendLG","data":142672009,"nbits":28}`;
const LED_CONTROL1 = data => `/request?data=${JSON.stringify({type:"LedControl1", control:"LED", data})}`;

var dhtInfo;
function updateDHT11Info() {
    return fetch(URL + DHT11_1)
    .then(res => res.json());
}


function turnOnAircon() {
    return fetch(URL + TURN_ON_AIRCON)
    .then(res => res.json()).then(json => {
        console.log("TurnOnAircon: ", json);
        airconStatus = true;
    });
}
function turnOffAircon() {
    return fetch(URL + TURN_OFF_AIRCON)
    .then(res => res.json()).then(json => {
        console.log("TurnOffAircon: ", json);
        airconStatus = false;
    });
}
function powerModeAircon() {
    return fetch(URL + POWER_MODE_AIRCON)
    .then(res => res.json()).then(json => {
        console.log("PowerModeAircon: ", json);
    });
}


const MAX_T = 25.1;
const MIN_T = 24.9;

var ANIMATION1 = [2172748161, 3284386755, 3890735079, 4294967295, 3890735079, 3284386755, 2172748161, 0].map(e => new Array(8).fill(e));
var clockLock = false;
var airconStatus;
var dhtInfo;
setInterval(() => {
    updateDHT11Info().then(json => {
        if (json.status == "success") {
            var isUnderMin = json.hic <= MIN_T;
            var isOverMax = json.hic >= MAX_T;
            var delta = 0;
            if (dhtInfo) {
                delta = json.hic - dhtInfo.hic;
            }
            if (isUnderMin) {
                if (airconStatus) {
                    turnOffAircon();
                } else if (delta < 0) {
                    // !airconStatus
                    turnOffAircon();
                    console.log('Delta decreased but airconStatus is off...');
                }
            } else if (isOverMax) {
                if (!airconStatus) {
                    turnOnAircon().then(() => powerModeAircon());
                } else if (delta > 0) {
                    // airconStatus
                    turnOnAircon().then(() => powerModeAircon());
					console.log('Delta increased but airconStatus is on...');
                }
            }
            dhtInfo = json;
            clockLock = true;
            sendLedAnimation(ANIMATION1).then(_ =>new Promise((r)=>{
					sendText(`T:${String(json.t).substring(0, 6)}`);
					setTimeout(_ => r(), 5000);
				})).then(_ => clockLock = false).catch(_ => clockLock = false);
        }
        console.log(json);
        // Request DotUpdate
    }).catch(e => {
        console.error(e);
        console.log("DHTInfo Update Error!");
    });
}, 30 * 1000);

turnOffAircon();

setInterval(_ => {
    if (!clockLock) {
        sendLedControl(getTimeString().split('').map(remapper));
    }
}, 1000)
/*
var i = 0;
var p = Promise.resolve();
setInterval(_=>{
p = p.then(_=>sendText(String(i++).substring(0,8)));
}, 100);
 */

/*
d = d => fetch("http://192.168.0.123/request?data={%22type%22:%22LedControl1%22,%22control%22:%22LED%22,%22data%22:" + JSON.stringify(d) + "}")
dd = d => fetch("http://192.168.0.123/read?data={%22type%22:%22Arduino1QueueClear%22}")
setInterval(_ => {
var t = parseLinarArray(getTimeArray());
console.log(t);
d(t)

}, 1000)
/*
setInterval(_=>{
dd()

},1000*10)
*/
