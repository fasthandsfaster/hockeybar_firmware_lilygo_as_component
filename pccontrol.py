#!/usr/bin/env python

import asyncio
from random import random
from websockets.server import serve
import json
import time

send_queue = asyncio.Queue()
recv_queue = asyncio.Queue()
game_queue = asyncio.Queue()

elementDict = {}

 
# coroutine to consume work
async def game():
    print('Game: Running')
    resultList = []
    # consume work

    while True:
        # get a unit of wokr
        item = await recv_queue.get()
        print("item: %s",item)
        if item is None:
            break
        elif item["newElement"] == True:
            elementDict = item["allElementsDict"][1]
            startDict = { 
                "deviceId": "999",
                "messageId": "ABC",
                "timestamp": "12:00:00",
                "elementId": 99,
                "payload": 
                {
                    "task": "start"
                }
            }
            jsontxt = json.dumps(startDict)
            print("startJson: %s\n",jsontxt)
            await elementDict["websocket"].send(jsontxt+"\0")

            setDict = {
                "deviceId": "999",
                "messageId": "ABC",
                "timestamp": "12:00:00",
                "elementId": 99,
                "payload": 
                {
                    "task": "set"
                }
            }
            jsontxt = json.dumps(setDict)
            print("setJson: %s\n",jsontxt)
            await elementDict["websocket"].send(jsontxt+"\0")

        elif "solved" in item["jsonDict"]:
            resultList.append(item["jsonDict"])
            print("Result: %s\n",jsonDict)


    print('Game: Running')
    # consume work
    while True:
        # get a unit of work
        item = await recv_queue.get()
        # check for stop signal
        if item is None:
            break
        # report
        print(f'>got {item}')
    # all done
    print('Consumer: Done')

async def ws_input_handler(websocket):
    print("WS Handler started")
    allElementsDict = {}
    async for message in websocket:         
        jsondict = json.loads(message)
        print(jsondict)
        if "connected" in jsondict["payload"]:
                if jsondict["payload"]["connected"] == True:
                    print("Element connected")
                    elementDict = {
                        "elementId": 1,
                        "deviceId": jsondict["deviceId"],
                        "type": jsondict["payload"]["type"],
                        "hwVersion": jsondict["payload"]["hwVersion"],
                        "swVersion": jsondict["payload"]["swVersion"],
                        "websocket": websocket 
                    }
                    allElementsDict[elementDict["elementId"]] = elementDict
                    newElement = True
                    initDict = {
                    	"task":"init",
                        "elementId": 1
                    }
                    jsontxt = json.dumps(jsondict)
                    print("initJson: %s\n",jsontxt)
 
                    await recv_queue.put({"newElement":True,"allElementsDict":allElementsDict,"jsonDict":jsondict})
        
        elif "solved" in jsondict:
            await recv_queue.put({"newElement":False,"allElementsDict":None,"jsonDict":jsondict})
    
   
    

async def main():
    echostart = time.time()

    asyncio.create_task(game())              

    async with serve(ws_input_handler,"192.168.8.123", 8080):
        await asyncio.Future()  # run forever

    echotime = time.time() - echostart
    print("Total time: " + str(echotime))


asyncio.run(main())
""" **********************************************************************
# SuperFastPython.com
# example of using an asyncio queue
from random import random
import asyncio
 
# coroutine to generate work
async def producer(queue):
    print('Producer: Running')
    # generate work
    for i in range(10):
        # generate a value
        value = random()
        # block to simulate work
        await asyncio.sleep(value)
        # add to the queue
        await queue.put(value)
    # send an all done signal
    await queue.put(None)
    print('Producer: Done')
 
# coroutine to consume work
async def consumer(queue):
    print('Consumer: Running')
    # consume work
    while True:
        # get a unit of work
        item = await queue.get()
        # check for stop signal
        if item is None:
            break
        # report
        print(f'>got {item}')
    # all done
    print('Consumer: Done')
 
# entry point coroutine
async def main():
    # create the shared queue
    queue = asyncio.Queue()
    # run the producer and consumers
    await asyncio.gather(producer(queue), consumer(queue))
 
# start the asyncio program
asyncio.run(main())
"""