#!/usr/bin/env python

import asyncio
from websockets.server import serve
import json
import time

echojson = {
    "deviceId": "999",
    "messageId": "ABC",
    "timestamp": "12:00:00",
    "elementId": 99,
    "payload": {
        "task": "echo",
        "index": 1
     },
}
 
echotxt = json.dumps(echojson)
print(echotxt)

async def echo(websocket):
    async for message in websocket:
        print("Message: %s\n",message)
        jsondict = json.loads(message)
        print(jsondict)
        if "task" in jsondict["payload"]:
            task = jsondict["payload"]["task"]
            print("task:" + task)
            match task:
                case "init":
                    print("init")
                    echojson = {
                        "deviceId": "999",
                        "messageId": "ABC",
                        "timestamp": "12:00:00",
                        "elementId": 99,
                        "payload": {
                            "task": "echo",
                            "index": 1
                        }
                    }
                    echojsontxt = json.dumps(echojson)
                    print("echojsontxt: %s\n",echojsontxt)
                    await websocket.send(echojsontxt+"\0")
            
                case "echo":
                    if jsondict["payload"]["index"] < 100:
                        jsondict["payload"]["index"] = jsondict["payload"]["index"]+1
                        jsontxt = json.dumps(jsondict)
                        print("message: %s\n",jsontxt)
                        await websocket.send(jsontxt+"\0")
                    else:
                        raise Exception('echoloopingdone')

        else:
            print("initinit")
            echojson = {
                "deviceId": "999",
                "messageId": "ABC",
                "timestamp": "12:00:00",
                "elementId": 99,
                "payload": {
                    "task": "echo",
                    "index": 1
                }
            }
            echojsontxt = json.dumps(echojson)
            print("echojsontxt: %s\n",echojsontxt)
            await websocket.send(echojsontxt+"\0")
    

async def main():
    echostart = time.time()
    try:
        async with serve(echo,"192.168.8.123", 8080):
            await asyncio.Future()  # run forever
    except echoloopingdone:
        print("Loop done")
               
    echotime = time.time() - echostart
    print("Total time: " + str(echotime))


asyncio.run(main())

