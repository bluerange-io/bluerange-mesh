FruityGate (nRF)
==
FruityGate allows you to connect multiple mesh networks together using http.

There are two halves to FruityGate: a [nodejs app](https://github.com/microcosm/fruitygate-nodejs), and an nRF app (this repo). The nodejs app runs on any internet-enabled computer, and talks over USB to a [NRF51 device](https://www.nordicsemi.com/eng/Products/nRF51-Series-SoC) (like this dongle) which is running the nRF app.

![An nRF51 dongle](/img/nRF51_PCA10031.jpg)

When the nRF device is a member of a [fruity](https://github.com/mwaylabs/fruitymesh/wiki)-based mesh network, it will work with the nodejs app to act as a gateway which can talk to other similar gateways. In this way you can share messages between multiple bluetooth mesh networks over http.

The meshes can be in the same building or on the other side of the world.

How to use it
--
Clone this repo, compile and flash onto any nRF51 device that you want to be one half of a gateway. Plug the device into a computer, and follow the [serial-gateway-node](https://github.com/microcosm/serial-gateway-node) instructions on the computer to connect the nodejs app.

Yes. But how to *really* use it?
--
Oh, you want to actually *do* something with it? That makes sense.

Typically the reason you want to use it is because you are writing a custom nRF51 device application for a node, and you want to allow that node to talk to nodes on remote meshes. In other words, to *really* use it, you need instructions on how to code the custom device that will *talk to* the gateway, not the gateway itself.

Your custom app should still be based on the standard [fruitymesh](https://github.com/mwaylabs/fruitymesh) code, no need to pull in anything from the gateway repo. You just need your custom app to construct and send network messages the right way *to* a gateway node.

One (minor) change
--
First up, you will need to make one minor modification to the standard fruitymesh code you have cloned for your custom app. In `/config/conn_packets.h`, around line 83 you will see this:

```cpp
#define SIZEOF_CONN_PACKET_HEADER 5
typedef struct
{
	u8 hasMoreParts : 1; //Set to true if message is split and has more data in the next packet
	u8 messageType : 7;
	nodeID sender;
	nodeID receiver;
}connPacketHeader;
```

You will need to replace that code block with this:

```cpp
#define SIZEOF_CONN_PACKET_HEADER 6
typedef struct
{
	u8 hasMoreParts : 1; //Set to true if message is split and has more data in the next packet
	u8 messageType : 7;
	nodeID sender;
	nodeID receiver;
	nodeID remoteReceiver;
}connPacketHeader;
```

That is, you will need to add `nodeID remoteReceiver;` to the bottom, and up the `SIZEOF_CONN_PACKET_HEADER` to `6`.

The reason is because the messages you send will now have to target two nodes: the local gateway device which will forward the message over http, and the node on the remote network you are ultimately aiming for.

Note that this will increase the size of all packet headers for all modules, since this packet header is shared. This reduces the possible size of all single-packet data by one byte. Bummer.

Use the gateway in a custom module
--
Now that you have adjusted the header you can code custom modules as described in the [fruitymesh wiki](https://github.com/mwaylabs/fruitymesh/wiki/Implementing-a-Custom-Module). There are two things you will have to do differently:

1. Rather than having your custom module reference *itself* in data exchanges, it will reference the gateway module on the gateway device, which has a fixed `moduleID` of `30999`.
2. You have to set the `remoteReceiver` property of the packet header, which you added above.

Other than that, the process is the same. Still, I have written some examples to give you a leg up.

Example: send a string
--
Wherever in your custom module you are ready to send a message over a gateway, you can construct and send the message like this:

```cpp
string customString = "custom";
connPacketModuleAction packet;
packet.header.messageType = MESSAGE_TYPE_MODULE_TRIGGER_ACTION;
packet.header.sender = node->persistentConfig.nodeId;
packet.header.receiver = gatewayNodeId;
packet.header.remoteReceiver = remoteNodeId; // <=== SET THE REMOTE NODE ID
packet.moduleId = 30999; // <=== SET THE GATEWAY MODULE ID

vector<u8> convert(customString.begin(), customString.end());
for(int i = 0; i < convert.size(); i++) {
    packet.data[i] = convert[i];
}

cm->SendMessageToReceiver(NULL, (u8*)&packet, SIZEOF_CONN_PACKET_MODULE_ACTION + customString.length() + 1, true);
```

This example shows you how to convert a string into a `u8` ([aka](https://github.com/mwaylabs/fruitymesh/blob/master/config/types.h) `uint8_t`) byte array, for sending. Of course you can use decimal numbers or whatever you like in that array instead, so long as the node that receives it knows what to do with it.

Some of the variables in the example above are context-dependent on example implementations from the [sample ping module](https://github.com/mwaylabs/fruitymesh/wiki/Implementing-a-Custom-Module). If you need to dig deeper, check out this commit for a [full example implementation](https://github.com/microcosm/serial-gateway-fruitymesh/commit/878422af09593218dc347b660ca09d05bc720368).

Example: receive a string
--
Receiving messages *from* the gateway will always take place in `ConnectionPacketReceivedEventHandler`, and will look something like this:

```cpp
void MyCustomModule::ConnectionPacketReceivedEventHandler(connectionPacket* inPacket, Connection* connection, connPacketHeader* packetHeader, u16 dataLength)
{
	Module::ConnectionPacketReceivedEventHandler(inPacket, connection, packetHeader, dataLength);

	if(packetHeader->messageType == MESSAGE_TYPE_MODULE_TRIGGER_ACTION){
		connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

		if(packet->moduleId == 30999) // <=== NOTE THE GATEWAY MODULE ID
		{
			connPacketModuleAction* packet = (connPacketModuleAction*)packetHeader;

			string message = "";
			for(int i = 0; i < sizeof(packet->data); i++) {
			    message += packet->data[i];
			}

			logt("MYCUSTOMMOD", "Inbound message received from gateway: '%s'", message.c_str());
		}
	}
}
```

This code will read in the byte array as a string and print it. Of course if your byte array is not a string you will want to do something else with it instead.

Can you say that again but quicker?
--
Yes. For quick reference, you can see what's involved in implementing a gateway-compatible custom module in [this commit diff](https://github.com/microcosm/serial-gateway-fruitymesh/commit/878422af09593218dc347b660ca09d05bc720368). The only thing not covered in that diff is the packet header stuff above - don't forget to do that!

This doesn't work. You suck.
--
Yes, I do a bit. Bear in mind everything above is in the context of [this specific commit](https://github.com/mwaylabs/fruitymesh/commit/ba668a2d4a206bd93562c7c89f4369e806f678df), the latest at the time of writing. If you hit snags maybe try reverting to that before coding your custom module.

Other factors come in, such as the specific hardware. But ultimately this is new space, so it can be harder while the tools are in flux. Good luck.