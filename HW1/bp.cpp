/* 046267 Computer Architecture - HW #1                                 */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"
#include <iostream>
#include <vector>
#include <cmath>
using namespace std;

enum shareType { NO_SHARE = 0, SHARE_LSB = 1, SHARE_MID = 2};

//State Machine Defs:
//00 - SNT, 01 - WNT, 10 - WT, 11 - ST.

class BP {
    public:
    unsigned btbSize;
    unsigned historySize;
    unsigned tagSize;
    unsigned fsmState;
    bool isGlobalHist;
    bool isGlobalTable;
    int shareType; //0: NO_SHARE, 1: SHARE_LSB, 2: SHARE_MID

    vector<uint32_t> btbTags;
    vector<uint32_t> btbTargets;
    vector<uint32_t> historyTable;
    vector<uint8_t> fsmTable;
    vector<bool> btbValid;

    SIM_stats stats;
    BP(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
       bool isGlobalHist, bool isGlobalTable, int shareType)
    : btbSize(btbSize), historySize(historySize), tagSize(tagSize), fsmState(fsmState), isGlobalHist(isGlobalHist), isGlobalTable(isGlobalTable), shareType(shareType){
    stats.flush_num = 0;
    stats.br_num = 0;
    stats.size = 0;

    // 1. Resize BTB Arrays
    btbTags.resize(btbSize, 0);
    btbTargets.resize(btbSize, 0);
    btbValid.resize(btbSize, false);

    // 2. Resize History Table
    // If Global History: we need just 1 entry.
    // If Local History: we need 'btbSize' entries.
    if (isGlobalHist) historyTable.resize(1, 0);
    else              historyTable.resize(btbSize, 0);

    // 3. Resize FSM Table (The 2-bit counters)
    // How many counters total? 2^historySize.
    // If Global Table: Just 1 set of counters. Total = 2^historySize.
    // If Local Table: 'btbSize' sets of counters. Total = btbSize * 2^historySize.
    int numEntries = 1 << historySize;
    if (isGlobalTable) fsmTable.resize(numEntries, fsmState);
    else               fsmTable.resize(btbSize * numEntries, fsmState);
    }
};

// Global Pointer
BP* bp = nullptr;

// --------------------------------------------------------------------
// Helper: Get the pointer to the correct FSM counter
// --------------------------------------------------------------------
// This function handles the math to find the right counter in the 1D array
uint8_t* getFsmCounter(uint32_t btbIdx, uint32_t history, uint32_t pc) {

    // 1. Calculate Index based on Sharing (XOR)
    uint32_t index = history;
    if (bp->shareType == 1) { // LSB
        index = index ^ ((pc >> 2) & ((1 << bp->historySize) - 1));
    } else if (bp->shareType == 2) { // Mid
        index = index ^ ((pc >> 16) & ((1 << bp->historySize) - 1));
    }

    // 2. Locate in the Table
    if (bp->isGlobalTable) {
        // Global Table: Just use the calculated index
        return &bp->fsmTable[index];
    } else {
        // Local Table: We must offset by the btbIndex
        // Formula: (btbRow * entriesPerRow) + index
        int entriesPerRow = 1 << bp->historySize;
        return &bp->fsmTable[(btbIdx * entriesPerRow) + index];
    }
}


// --------------------------------------------------------------------
// API Implementation
// --------------------------------------------------------------------

int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
            bool isGlobalHist, bool isGlobalTable, int Shared) {
    if (bp) delete bp;
    bp = new BP(btbSize, historySize, tagSize, fsmState, isGlobalHist, isGlobalTable, Shared);
    return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst) {
    // 1. Calculate BTB Index and Tag
    uint32_t btbIdx = (pc >> 2) & (bp->btbSize - 1);
    uint32_t tag = (pc >> (2 + (int)log2(bp->btbSize))) & ((1 << bp->tagSize) - 1);

    // 2. Check BTB Hit
    if (bp->btbValid[btbIdx] && bp->btbTags[btbIdx] == tag) {

        // 3. Get History and FSM
        uint32_t hist = bp->isGlobalHist ? bp->historyTable[0] : bp->historyTable[btbIdx];
        uint8_t* statePtr = getFsmCounter(btbIdx, hist, pc);

        bool prediction = (*statePtr >= 2); // Taken if 2(WT) or 3(ST)

        if (prediction) {
            *dst = bp->btbTargets[btbIdx];
            return true;
        } else {
            *dst = pc + 4; // Spec Requirement: If NT, return PC+4
            return false;
        }
    }

    // Miss
    *dst = pc + 4;
    return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst) {
    bp->stats.br_num++;

    // 1. Calculate BTB Index and Tag
    uint32_t btbIdx = (pc >> 2) & (bp->btbSize - 1);
    uint32_t tag = (pc >> (2 + (int)log2(bp->btbSize))) & ((1 << bp->tagSize) - 1);

    // 2. Update Flush Stats
    // Check if our prediction was wrong OR if target was wrong
    bool predictedTaken = (pred_dst != pc + 4);
    if ((predictedTaken != taken) || (taken && pred_dst != targetPc)) {
        bp->stats.flush_num++;
    }

    // 3. Handle BTB Conflict / New Entry
    // If invalid, OR tag mismatch -> We are overwriting/initing this entry
    if (!bp->btbValid[btbIdx] || bp->btbTags[btbIdx] != tag) {

        bp->btbValid[btbIdx] = true;
        bp->btbTags[btbIdx] = tag;

        // Reset Local History (if applicable)
        if (!bp->isGlobalHist) {
            bp->historyTable[btbIdx] = 0;
        }

        // Reset Local Table (if applicable)
        if (!bp->isGlobalTable) {
            // We need to reset the WHOLE row for this BTB entry to default state
            int entriesPerRow = 1 << bp->historySize;
            int startIdx = btbIdx * entriesPerRow;
            for(int i = 0; i < entriesPerRow; i++) {
                bp->fsmTable[startIdx + i] = bp->fsmState;
            }
        }
    }
    // Always update target
    bp->btbTargets[btbIdx] = targetPc;

    // 4. Update FSM State
    uint32_t hist = bp->isGlobalHist ? bp->historyTable[0] : bp->historyTable[btbIdx];
    uint8_t* statePtr = getFsmCounter(btbIdx, hist, pc);

    if (taken) {
        if (*statePtr < 3) (*statePtr)++;
    } else {
        if (*statePtr > 0) (*statePtr)--;
    }

    // 5. Update History
    // Shift left, add taken bit (1 or 0)
    uint32_t newHist = ((hist << 1) | (taken ? 1 : 0)) & ((1 << bp->historySize) - 1);

    if (bp->isGlobalHist) bp->historyTable[0] = newHist;
    else                  bp->historyTable[btbIdx] = newHist;
}

void BP_GetStats(SIM_stats *curStats) {
    *curStats = bp->stats;

    // Calculate Storage Size in bits
    // BTB: Entries * (Valid + Tag + Target)
    unsigned btbBits = bp->btbSize * (1 + bp->tagSize + 30);

    // History:
    unsigned histBits = bp->historyTable.size() * bp->historySize;

    // FSM:
    unsigned fsmBits = bp->fsmTable.size() * 2; // 2 bits per state

    curStats->size = btbBits + histBits + fsmBits;

    delete bp;
}



