/* vim: set expandtab ts=4 sw=4: */
/*
 * You may redistribute this program and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "net/Ducttape.h"
#include "dht/DHTModuleRegistry.h"
#include "dht/dhtcore/Router.h"
#include "dht/dhtcore/RumorMill.h"
#include "switch/SwitchCore.h"
#include "memory/Allocator.h"
#include "tunnel/IpTunnel.h"
#include "wire/Headers.h"
#include "wire/SwitchHeader.h"
#include "util/events/EventBase.h"
#include "util/Linker.h"
Linker_require("net/Ducttape.c")

struct Ducttape
{
    struct Interface_Two switchIf;
    struct Interface_Two tunIf;
    struct Interface_Two dhtIf;

    struct Interface magicInterface;
    struct SessionManager* sessionManager;
};

struct Ducttape* Ducttape_register(uint8_t privateKey[32],
                                   struct DHTModuleRegistry* registry,
                                   struct Router* router,
                                   struct SwitchCore* switchCore,
                                   struct EventBase* eventBase,
                                   struct Allocator* allocator,
                                   struct Log* logger,
                                   struct IpTunnel* ipTun,
                                   struct Random* rand);

/**
 * Set the interface which the user will use to communicate with the network.
 *
 * @param dt the ducttape struct.
 * @param userIf the (TUN) interface which will be used to send and receive packets.
 */
void Ducttape_setUserInterface(struct Ducttape* dt, struct Interface* userIf);

/**
 * The structure of data which should be the beginning
 * of the content in the message sent to injectIncomingForMe.
 */
struct Ducttape_IncomingForMe
{
    struct SwitchHeader switchHeader;
    struct Headers_IP6Header ip6Header;
};

/**
 * Inject a packet into the stream of packets destine for this node.
 * The message must contain switch header, ipv6 header, then content.
 * None of it should be encrypted and there should be no CryptoAuth headers.
 */
uint8_t Ducttape_injectIncomingForMe(struct Message* message,
                                     struct Ducttape* ducttape,
                                     uint8_t herPublicKey[32]);



struct Ducttape* Ducttape_register(uint8_t privateKey[32],
                                   struct DHTModuleRegistry* registry,
                                   struct Router* router,
                                   struct SwitchCore* switchCore,
                                   struct EventBase* eventBase,
                                   struct Allocator* allocator,
                                   struct Log* logger,
                                   struct IpTunnel* ipTun,
                                   struct Random* rand)
{
    struct Ducttape_pvt* context = Allocator_calloc(allocator, sizeof(struct Ducttape_pvt), 1);
    context->registry = registry;
    context->router = router;
    context->logger = logger;
    context->eventBase = eventBase;
    context->alloc = allocator;
    Bits_memcpyConst(&context->pub.magicInterface, (&(struct Interface) {
        .sendMessage = magicInterfaceSendMessage,
        .allocator = allocator
    }), sizeof(struct Interface));
    Identity_set(context);

    context->ipTunnel = ipTun;

    ipTun->nodeInterface.receiveMessage = sendToNode;
    ipTun->nodeInterface.receiverContext = context;
    ipTun->tunInterface.receiveMessage = sendToTun;
    ipTun->tunInterface.receiverContext = context;

    struct CryptoAuth* cryptoAuth =
        CryptoAuth_new(allocator, privateKey, eventBase, logger, rand);
    Bits_memcpyConst(context->myAddr.key, cryptoAuth->publicKey, 32);
    Address_getPrefix(&context->myAddr);

    context->sm = SessionManager_new(incomingFromCryptoAuth,
                                     outgoingFromCryptoAuth,
                                     context,
                                     eventBase,
                                     cryptoAuth,
                                     rand,
                                     allocator);
    context->pub.sessionManager = context->sm;

    Bits_memcpyConst(&context->module, (&(struct DHTModule) {
        .name = "Ducttape",
        .context = context,
        .handleOutgoing = handleOutgoing
    }), sizeof(struct DHTModule));

    Bits_memcpyConst(&context->switchInterface, (&(struct Interface) {
        .sendMessage = incomingFromSwitch,
        .senderContext = context,
        .allocator = allocator
    }), sizeof(struct Interface));

    if (DHTModuleRegistry_register(&context->module, context->registry)
        || SwitchCore_setRouterInterface(&context->switchInterface, switchCore))
    {
        return NULL;
    }

    // setup the switch pinger interface.
    Bits_memcpyConst(&context->pub.switchPingerIf, (&(struct Interface) {
        .sendMessage = incomingFromPinger,
        .senderContext = context
    }), sizeof(struct Interface));

    return &context->pub;
}