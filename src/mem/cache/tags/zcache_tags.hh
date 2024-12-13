/*
 * Copyright (c) 2012-2014,2017 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2003-2005,2014 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * Declaration of a base set associative tag store.
 */

#ifndef __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__
#define __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "base/logging.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/packet.hh"
#include "params/ZcacheTags.hh"

#include "base/trace.hh"
#include "debug/Zcache.hh"

namespace gem5
{

/**
 * A basic cache tag store.
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 *
 * The ZcacheTags placement policy divides the cache into s sets of w
 * cache lines (ways).
 */
class ZcacheTags : public BaseTags
{
  protected:
    /** The allocatable associativity of the cache (alloc mask). */
    unsigned allocAssoc;

    /** The cache blocks. */
    std::vector<CacheBlk> blks;

    /** Whether tags and data are accessed sequentially. */
    const bool sequentialAccess;

    /** Replacement policy */
    replacement_policy::Base *replacementPolicy;

  public:
    /** Convenience typedef. */
     typedef ZcacheTagsParams Params;

    /**
     * Construct and initialize this tag store.
     */
    ZcacheTags(const Params &p);

    /**
     * Destructor
     */
    virtual ~ZcacheTags() {};

    /**
     * Initialize blocks as CacheBlk instances.
     */
    void tagsInit() override;

    /**
     * This function updates the tags when a block is invalidated. It also
     * updates the replacement data.
     *
     * @param blk The block to invalidate.
     */
    void invalidate(CacheBlk *blk) override;

    /**
     * Access block and update replacement data. May not succeed, in which case
     * nullptr is returned. This has all the implications of a cache access and
     * should only be used as such. Returns the tag lookup latency as a side
     * effect.
     *
     * @param pkt The packet holding the address to find.
     * @param lat The latency of the tag lookup.
     * @return Pointer to the cache block if found.
     */
    CacheBlk* accessBlock(const PacketPtr pkt, Cycles &lat) override
    {
        CacheBlk *blk = findBlock(pkt->getAddr(), pkt->isSecure());

        // Access all tags in parallel, hence one in each way.  The data side
        // either accesses all blocks in parallel, or one block sequentially on
        // a hit.  Sequential access with a miss doesn't access data.
        stats.tagAccesses += allocAssoc;
        if (sequentialAccess) {
            if (blk != nullptr) {
                stats.dataAccesses += 1;
            }
        } else {
            stats.dataAccesses += allocAssoc;
        }

        // If a cache hit
        if (blk != nullptr) {
            // Update number of references to accessed block
            blk->increaseRefCount();

            // Update replacement data of accessed block
            replacementPolicy->touch(blk->replacementData, pkt);
        }

        // The tag lookup latency is the same for a hit or a miss
        lat = lookupLatency;

        return blk;
    }

    /**
     * Find replacement victim based on address. The list of evicted blocks
     * only contains the victim.
     *
     * @param addr Address to find a victim for.
     * @param is_secure True if the target memory space is secure.
     * @param size Size, in bits, of new block to allocate.
     * @param evict_blks Cache blocks to be evicted.
     * @return Cache block to be replaced.
     */
    CacheBlk* findVictim(Addr addr, const bool is_secure,
                         const std::size_t size,
                         std::vector<CacheBlk*>& evict_blks) override
    {
        // Get possible entries to be victimized
        // used to be const

        // generates the first level parents
        std::vector<ReplaceableEntry*> entries =
            indexingPolicy->getPossibleEntries(addr);

        // generates the first level parents
        // std::vector<ReplaceableEntry*> entries =
        //     zcachePolicy->getPossibleEntries(addr);
        
        std::vector<ReplaceableEntry*> entries_level2;

        // since parents were pushed in order of way, this will be the parent way
        int parentWay = 0;
        for (const auto& it: entries) {

            // Make a cacheblk out of the parernt entry
            CacheBlk* parentBlk = static_cast<CacheBlk*>(it);

            if (parentBlk->getTag() != MaxAddr) {
                // DPRINTF(Zcache, "Tag is not max address.\n");
                std::vector<ReplaceableEntry*>entries_level2_for_a_parent;
                Addr parentAddr = regenerateBlkAddr(parentBlk);
                entries_level2_for_a_parent = indexingPolicy->getPossibleEntriesSecond(parentAddr, it, parentWay);
                // entries_level2_for_a_parent = zcachePolicy->getPossibleEntriesSecond(parentAddr, it, parentWay);

                for (const auto& level2s: entries_level2_for_a_parent) {
                    entries_level2.push_back(level2s);
                }
            }
            parentWay++;
        }

        for (const auto& it3: entries_level2) {
            entries.push_back(it3);
        }

        // for (auto it3: entriesl1) {
        //     entries.push_back(it3);
        // }


        // Choose replacement victim from replacement candidates
        CacheBlk* victim = static_cast<CacheBlk*>(replacementPolicy->getVictim(
                                entries));

        CacheBlk* victimParent = static_cast<CacheBlk*>(victim->getParentEntry());

        if (victim->getParentEntry() != NULL) {
            // swap with the parent
            // DPRINTF(Zcache, "Swap happened.\n");
            std::swap(victim, victimParent);
            evict_blks.push_back(victim);
            return victim;
            // evict_blks.push_back(victimParent);
            // return victimParent;
        }
        else {
            evict_blks.push_back(victim);
        }
        
        // DPRINTF(Zcache, "Printing cache:\n%s", BaseTags::print());
        // if (victim->getTag() != 0xffffffffffffffff) {
        //     DPRINTF(Zcache, "Non-zero tag in l2: %x\n", victim->getTag());
        // }
        // DPRINTF(Zcache, "Tag of victim block: %x\n", victim->getTag());
        // DPRINTF(Zcache, "Address of victim block: %x\n", regenerateBlkAddr(victim));
        

        // There is only one eviction for this replacement

        return victim;
    }

    /**
     * Insert the new block into the cache and update replacement data.
     *
     * @param pkt Packet holding the address to update
     * @param blk The block to update.
     */
    void insertBlock(const PacketPtr pkt, CacheBlk *blk) override
    {
        // Insert block
        BaseTags::insertBlock(pkt, blk);
        // DPRINTF(Zcache, "Inserting address: %x\n", pkt->getAddr());


        // Increment tag counter
        stats.tagsInUse++;

        // Update replacement policy
        replacementPolicy->reset(blk->replacementData, pkt);
    }

    void moveBlock(CacheBlk *src_blk, CacheBlk *dest_blk) override;

    /**
     * Limit the allocation for the cache ways.
     * @param ways The maximum number of ways available for replacement.
     */
    virtual void setWayAllocationMax(int ways) override
    {
        fatal_if(ways < 1, "Allocation limit must be greater than zero");
        allocAssoc = ways;
    }

    /**
     * Get the way allocation mask limit.
     * @return The maximum number of ways available for replacement.
     */
    virtual int getWayAllocationMax() const override
    {
        return allocAssoc;
    }

    /**
     * Regenerate the block address from the tag and indexing location.
     *
     * @param block The block.
     * @return the block address.
     */
    Addr regenerateBlkAddr(const CacheBlk* blk) const override
    {
        return indexingPolicy->regenerateAddr(blk->getTag(), blk);
        // return zcachePolicy->regenerateAddr(blk->getTag(), blk);
    }

    void forEachBlk(std::function<void(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            visitor(blk);
        }
    }

    bool anyBlk(std::function<bool(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            if (visitor(blk)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace gem5

#endif //__MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__