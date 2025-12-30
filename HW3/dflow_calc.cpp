/* 046267 Computer Architecture - HW #3 */
/* Implementation (skeleton)  for the dataflow statistics calculator */

#include "dflow_calc.h"
#include <vector>
#include <algorithm>

#define NUM_OF_REGS 32

struct instNode {
    unsigned int opcode; // Each instruction opcode
    unsigned int dst; // Destination register
    unsigned int src1; // Source register 1
    unsigned int src2; // Source register 2

    // Dependency info (Index for each instruction, -1 if no dependency -> entry)
    int src1DepInst; // Dependency instruction for source 1
    int src2DepInst; // Dependency instruction for source 2

    // Depth info -> timing
    int depth; // Depth of the instruction in the dataflow graph
    int finishTime; // Finish time of the instruction
};

struct progContext {
    std::vector<instNode> insts;
};

ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts) {
    progContext* ctx = new progContext;
    ctx->insts.reserve(numOfInsts);

    // Initialize last written instruction for each register
    int lastWrittenBy[NUM_OF_REGS];
    for (int i = 0; i < NUM_OF_REGS; i++) {
        lastWrittenBy[i] = -1; // No instruction has written to this register yet
    }

    // Analyze each instruction in the program trace
    instNode node;
    for (unsigned int i = 0; i < numOfInsts; i++) {
        node.opcode = progTrace[i].opcode;
        node.dst = progTrace[i].dstIdx;
        node.src1 = progTrace[i].src1Idx;
        node.src2 = progTrace[i].src2Idx;

        // Dependency
        node.src1DepInst = lastWrittenBy[node.src1];
        node.src2DepInst = lastWrittenBy[node.src2];
        
        // Calculate depth and finish time
        int finishTimeSrc1 = 0;
        if (node.src1DepInst != -1) {
            finishTimeSrc1 = ctx->insts[node.src1DepInst].finishTime;
        }
        int finishTimeSrc2 = 0;
        if (node.src2DepInst != -1) {
            finishTimeSrc2 = ctx->insts[node.src2DepInst].finishTime;
        }

        node.depth = std::max(finishTimeSrc1, finishTimeSrc2);
        node.finishTime = node.depth + opsLatency[node.opcode];
        lastWrittenBy[node.dst] = i;
        ctx->insts.push_back(node);
    }
    return (ProgCtx)ctx;
}

void freeProgCtx(ProgCtx ctx) {
    progContext* pCtx = (progContext*)ctx;
    delete pCtx;
}

int getInstDepth(ProgCtx ctx, unsigned int theInst) {
    progContext* pCtx = (progContext*)ctx;
    if (theInst >= pCtx->insts.size()) {
        return -1; // Invalid instruction index
    }
    return pCtx->insts[theInst].depth;
}

int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst) {
    progContext* pCtx = (progContext*)ctx;
    if (theInst >= pCtx->insts.size()) {
        return -1; // Invalid instruction index
    }
    *src1DepInst = pCtx->insts[theInst].src1DepInst;
    *src2DepInst = pCtx->insts[theInst].src2DepInst;
    return 0;
}

int getProgDepth(ProgCtx ctx) {
    progContext* pCtx = (progContext*)ctx;
    int maxDepth = 0;
    for (unsigned int i = 0; i < pCtx->insts.size(); i++) {
        if (pCtx->insts[i].finishTime > maxDepth) {
            maxDepth = pCtx->insts[i].finishTime;
        }
    }
    return maxDepth;
}


