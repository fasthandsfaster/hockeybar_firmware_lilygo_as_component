// WebSocket Server
// FROM https://www.npmjs.com/package/websocket
const OS = require('os');
const FS = require('fs');
const Path = require('path');

const WebSocketServer = require('websocket').server;
const http = require('http');

const myLocalIp = Object.values(OS.networkInterfaces())
      .flat()
      .filter(n => n.family == "IPv4" && !n.internal)
      .find(e => e).address;

const LISTENER_PORT = 8080;
const serverUrl = `ws://${myLocalIp}:${LISTENER_PORT}`;

let connectionMap = new Map();
// let foreignAddress = "";

let echoTestRequestList = null;
let echoTestReplyList = null;
let echoTestLength = null;
let echoTestRunning = false;

const event_tests = [
    {
        desc: 'A to B short timeout',
        command: 'start',
        timeOpen: 1000,
        sim_events: [
            {delay: 800, event: 'TE_HIT_BEAM_A'},
            {delay:  20, event: 'TE_HIT_BEAM_B'},
        ],
        expect: {
            solved: true,
            duration: 820
        },
    },
    {
        desc: 'A to B long timeout',
        command: 'start',
        timeOpen: 10000,
        sim_events: [
            {delay: 9000, event: 'TE_HIT_BEAM_A'},
            {delay:  50, event: 'TE_HIT_BEAM_B'},
        ],
        expect: {
            solved: true,
            duration: 9200
        },
    },
    {
        desc: 'No second hit',
        command: 'start',
        timeOpen: 1000,
        sim_events: [
            {delay: 800, event: 'TE_HIT_BEAM_A'},
        ],
        expect: {
            solved: false,
            duration: 820
        },
    },
    {
        desc: 'No hits',
        command: 'start',
        timeOpen: 1000,
        expect: {
            solved: false,
            duration: 9200
        },
    },
    {
        desc: 'Duplicated hits',
        command: 'start',
        timeOpen: 1000,
        sim_events: [
            {delay: 500, event: 'TE_HIT_BEAM_A'},
            {delay: 100, event: 'TE_HIT_BEAM_A'},
            {delay: 100, event: 'TE_HIT_BEAM_A'},
            {delay: 200, event: 'TE_HIT_BEAM_B'},
            {delay: 200, event: 'TE_HIT_BEAM_B'},
            {delay: 200, event: 'TE_HIT_BEAM_B'},
        ],
        expect: {
            solved: true,
            duration: 1000
        },
    },
    {
        desc: 'B to A short timeout',
        command: 'start',
        timeOpen: 1000,
        sim_events: [
            {delay: 800, event: 'TE_HIT_BEAM_B'},
            {delay:  20, event: 'TE_HIT_BEAM_A'},
        ],
        expect: {
            solved: true,
            duration: 820
        },
    },
];

function getLocalTimestamp() {
    const d = new Date();
    const year = d.getFullYear().toString();
    const month = (d.getMonth()+1).toString().padStart(2,'0');
    const day = d.getDate().toString().padStart(2,'0');
    const hour = d.getHours().toString().padStart(2,'0');
    const minute = d.getMinutes().toString().padStart(2,'0');
    const second = d.getSeconds().toString().padStart(2,'0');
    const millis = d.getMilliseconds().toString().padStart(3,'0');
    return `${year}-${month}-${day} ${hour}:${minute}:${second}.${millis}`;
}

function LOG(message = '') {
    console.log(getLocalTimestamp() + ' ' + message);    
}

function httpServerHandler(request, response) {
    LOG(`Received request for ${request.url}`);
    response.writeHeader(200, {"Content-Type": 'text/html'});

    if (request.method == "POST") {
        let chunks = [];       
        request.on('data', chunk => chunks.push(chunk));        
        request.on('end', done => {
            const body = Buffer.concat(chunks).toString();
            
            if (request.headers['content-type'].match(/^application\/json/)) {
                LOG("BODY:\n"+ JSON.stringify(JSON.parse(body), null, 2));
            }
            else {
                const [match, command, param] = body.match(/^([^:]+):?(.*)$/i);
                if (!match) {
                    LOG(`No match for command: ${body}`);
                }
                else {
                    LOG(`Got command: ${command}, param: ${param}`);
                    
                    [...connectionMap.entries()].forEach(([fa, connection]) => {
                        if (!connection) {
                            LOG(`${fa}: *** Error: HockeyBar Firmware not connected.`);
                        }
                        else {
                            if (command == 'runseq') {
                                LOG(`${fa}: HockeyBar Firmware Sequence Test Started`);
                                runTest(fa, connection, event_tests);
                            }
                            else if (command == 'looptest') {
                                LOG(`${fa}: HockeyBar Firmware Loop Test Started`);
                                runTest(fa, connection, event_tests, 1000);
                            }                        
                            else if (command == 'onehit') {
                                LOG(`${fa}: HockeyBar Firmware One Hit Test Started`);
                                runTest(fa, connection, event_tests.slice(0, 1));
                            }                        
                            else if (command == 'onemiss') {
                                LOG(`${fa}: HockeyBar Firmware One Miss Test Started`);
                                runTest(fa, connection, event_tests.slice(3, 4));
                            }                        
                            else if (command == 'ledstrip') {
                                const [rgbstr, countstr] = param.split(/:/);
                                const rgb = parseInt(rgbstr, 16);
                                const count = parseInt(countstr);
                                LOG(`${fa}: HockeyBar led strip Test Started, rgb: ${rgb}, count: ${count}`);
                                const json = JSON.stringify({command, rgb, count});
                                LOG(`${fa}: json: ${json}`);
                                connection.sendUTF(json);
                            }
                            else if (command == 'echotest') {
                                if (echoTestRunning) {
                                    LOG(`${fa}: *** Error: HockeyBar Firmware Echo Test is in progress`);
                                }
                                else {
                                    const [countstr, delayMillisStr] = param.split(/:/);
                                    const count = parseInt(countstr) || 1;
                                    const delayMillis = parseInt(delayMillisStr) || 1;
                                    LOG(`${fa}: HockeyBar Firmware Echo Test Started, count: ${count}, delayMillis: ${delayMillis}`);
                                    echoTest(fa, connection, count, delayMillis);
                                }
                            }                        
                            else {
                                LOG(`Unknkown command: ${command}`);
                            }
                        }    
                    });
                }
            }
        });
    }    
    response.end();
}

const httpServer = http.createServer(httpServerHandler);

httpServer.listen(LISTENER_PORT, () => {
    LOG(`Server is listening on ${serverUrl}`);
});

const wsServer = new WebSocketServer({
    httpServer: httpServer,
    // You should not use autoAcceptConnections for production
    // applications, as it defeats all standard cross-origin protection
    // facilities built into the protocol and the browser.  You should
    // *always* verify the connection's origin and decide whether or not
    // to accept it.
    autoAcceptConnections: false
});

function originIsAllowed(origin) {
  // put logic here to detect whether the specified origin is allowed.
  return true;
}

function getRandomConnection(activeConnections) {
    const index =  Math.floor(Math.random() * activeConnections.length);
    return activeConnections[index];
}

function sendToTarget(fa, connection, event) {
    const timestamp = new Date().toJSON();
    const {command, timeOpen, sim_events, deviceId} = event;
    const data = {timestamp, command, timeOpen, sim_events, deviceId};
    const dataStr = JSON.stringify(data);
    LOG();
    LOG(`${fa}: TEST: ${event.desc}`)
    LOG(`${fa}: << ${dataStr}`);
    connection.sendUTF(dataStr);
}

function runTest(fa, connection, event_tests, repeat_count = 0) {
    let index = 0;
    let repeat_index = 0;
    
    const runAndWait = () => {
        if (index<event_tests.length) {
            if (index == 0 && repeat_count>0) {
                LOG()
                LOG(`${ip}: Test #: ${repeat_index} (${repeat_count})`)
            }
            const event = event_tests[index];
            const delaySum = (event.sim_events??[]).reduce((sum, e) => sum + e.delay, 0);
            const timeToWait = Math.max(event.timeOpen, delaySum) + 1000;
            sendToTarget(fa, connection, event);
            index++;
            setTimeout(runAndWait, timeToWait);
        }
        else {
            if (repeat_index < repeat_count) {
                repeat_index++;
                index = 0;
                runAndWait();
            }
        }
    };
    runAndWait();
}

function echoTest(fa, connection, count, delayMillis = 500) {
    echoTestRequestList = [];
    echoTestReplyList = [];
    echoTestLength = count;
    console.log("echoTestLength:", echoTestLength); // !DEBUG!

    let index = 0;
    const sendAndWait = () => {
        if (index < count) {
            const command = 'echo';
            const data = {command, index};
            const dataStr = JSON.stringify(data);
            LOG();
            LOG(`${fa}: ECHO TEST: ${index}`)
            LOG(`${fa}: << ${dataStr}`);
            echoTestRequestList.push({key: `${fa}-${index}`, timestampNanos: process.hrtime.bigint()});
            connection.sendUTF(dataStr);
            index++
            setTimeout(sendAndWait, delayMillis);
        }
    };
    echoTestRunning = true;
    sendAndWait();
}

function echoTestEvaluate() {

    console.log("echoTestRequestMap:", echoTestRequestList); // !DEBUG!
    console.log("echoTestReplyMap:", echoTestReplyList); // !DEBUG!
    
    const replyCount = BigInt(echoTestReplyList.length);
    let replyTimeSumMillis = 0n;

    for (const {key, timestampNanos} of echoTestRequestList) {
        const reply = echoTestReplyList.find(e => e.key == key);
        const replyTimestampNanos = reply.timestampNanos;

        const diffMillis = (replyTimestampNanos-timestampNanos)/1000000n;
        const line = `key: ${key}, requestTimestampNanos: ${timestampNanos}, replyTimestampNanos: ${replyTimestampNanos} }, diff-millis: ${diffMillis}`;
        LOG(line);
        replyTimeSumMillis += diffMillis;
    }
    const avarageReplyTimeMillis = replyTimeSumMillis / replyCount;
    LOG(`avarageReplyTimeMillis: ${avarageReplyTimeMillis}`);
    echoTestRunning = false;
}

function getForeignIp(connection) {
    const [match, ip] = connection.remoteAddress.match(/::[^:]*:(.*)/);
    return ip;
}

function sendInitialization(fa, connection, dataFromTarget) {
    const ip = getForeignIp(connection);
    const lastPartOfIp = dataFromTarget.ip.split(/\./).reverse().find(e => true);
    const deviceId = parseInt(lastPartOfIp);
    const initialization_event = {
        desc: 'Initialize target device',
        command: 'init',
        deviceId,
    }
    sendToTarget(fa, connection, initialization_event);
}

wsServer.on('request', request => {
    if (!originIsAllowed(request.origin)) {
        // Make sure we only accept requests from an allowed origin
        request.reject();
        LOG('Connection from origin ' + request.origin + ' rejected.');
        return;
    }        
    const connection = request.accept(null, request.origin);

    const fa = `${getForeignIp(connection)}:${request.socket._peername.port}`;
    connectionMap.set(fa, connection);
    LOG(`${fa}: CONNECTED request.resourceURL.path: ${request.resourceURL.path}`);

    connection.on('message', message => {

        if (message.type === 'utf8') {
            LOG(`${fa}: >> ${message.utf8Data}`);
            const msgData = JSON.parse(message.utf8Data);
            
            if (msgData.connected) {
                sendInitialization(fa, connection, msgData);
            }
            else if(msgData.type == "echoTest") { 
                echoTestReplyList.push({key: `${fa}-${msgData.index}`, timestampNanos: process.hrtime.bigint()});
                if (echoTestReplyList.length == echoTestLength) {
                    echoTestEvaluate();
                }
            }
        }
        else if (message.type === 'binary') {
            LOG(`${fa}: >> Binary message of ${message.binaryData.length} bytes`);
        }
    });

    connection.on('close', (reasonCode, description) => {
        LOG(`${fa}: CLOSE - disconnected`);
        connectionMap.delete(fa);
    });  
    // connection.on('ping', () => LOG('RECEIVED PING'));    
    // connection.on('pong', message => LOG(`RECEIVED PONG [${message.toString()}]`));
    connection.on('error', error => LOG(`${fa}: Got Connection Error: ${error.toString()}`));
});
