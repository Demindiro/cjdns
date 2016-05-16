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
#ifndef SupernodeHunter_H
#define SupernodeHunter_H

#include "memory/Allocator.h"
#include "util/log/Log.h"
#include "util/events/EventBase.h"
#include "crypto/random/Random.h"
#include "subnode/AddrSet.h"
#include "subnode/MsgCore.h"
#include "util/platform/Sockaddr.h"
#include "util/Linker.h"
Linker_require("subnode/SupernodeHunter.c");

struct SupernodeHunter
{
    struct AddrSet* snodes;
};

#define SupernodeHunter_addSnode_INVALID_FAMILY -1
#define SupernodeHunter_addSnode_EXISTS -2
int SupernodeHunter_addSnode(struct SupernodeHunter* snh, struct Sockaddr* udpAddr);

int SupernodeHunter_listSnodes(struct SupernodeHunter* snh,
                               struct Sockaddr*** out,
                               struct Allocator* alloc);

#define SupernodeHunter_removeSnode_NONEXISTANT -1
int SupernodeHunter_removeSnode(struct SupernodeHunter* snh, struct Sockaddr* toRemove);

 /**
  * The algorithm of this module is to ask each peer for a supernode using a findNode request.
  * If each peer comes up empty then each peer is sent a getPeers request and those nodes are
  * requested in turn.
  */
struct SupernodeHunter* SupernodeHunter_new(struct Allocator* allocator,
                                            struct Log* log,
                                            struct EventBase* base,
                                            struct AddrSet* peers,
                                            struct MsgCore* msgCore,
                                            struct Address* myAddress);

#endif
