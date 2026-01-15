/* 046267 Computer Architecture - HW #4 */

#include "core_api.h"
#include "sim_api.h"

#include <vector>
#include <iostream>
#include <cstring>

using namespace std;

enum ThreadStatus {
    READY,
    WAITING,
    HALTED
};

class Thread {
public:
    int id;
    tcontext context; // Registers
    uint32_t pc;
    ThreadStatus status;
    int wait_cycles; // Cycles remaining for memory operation

    Thread(int thread_id) : id(thread_id), pc(0), status(READY), wait_cycles(0) {
        for (int i = 0; i < REGS_COUNT; ++i) {
            context.reg[i] = 0;
        }
    }
};

class CoreSimulator {
    vector<Thread> threads;
    int num_threads;
    int switch_overhead;
    int load_latency;
    int store_latency;
    
    double total_cycles;
    int total_instructions;

public:
    CoreSimulator() {
        num_threads = SIM_GetThreadsNum();
        switch_overhead = SIM_GetSwitchCycles();
        load_latency = SIM_GetLoadLat();
        store_latency = SIM_GetStoreLat();
        total_cycles = 0;
        total_instructions = 0;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(i);
        }
    }

    double get_cpi() {
        if (total_instructions == 0) return 0;
        return total_cycles / total_instructions;
    }

    void get_thread_context(tcontext* ctxt, int threadid) {
        //main.c passes the array base, so we write to ctxt[threadid]
        if (threadid >= 0 && threadid < num_threads) {
             ctxt[threadid] = threads[threadid].context;
        }
    }

    // Execute one instruction for the specific thread
    void execute_instruction(Thread& t) {
        Instruction inst;
        SIM_MemInstRead(t.pc, &inst, t.id);
        
        // Advance PC for all except maybe branches
        t.pc++; 

        switch (inst.opcode) {
            case CMD_ADD:
                t.context.reg[inst.dst_index] = t.context.reg[inst.src1_index] + t.context.reg[inst.src2_index_imm];
                break;
            case CMD_SUB:
                t.context.reg[inst.dst_index] = t.context.reg[inst.src1_index] - t.context.reg[inst.src2_index_imm];
                break;
            case CMD_ADDI:
                t.context.reg[inst.dst_index] = t.context.reg[inst.src1_index] + inst.src2_index_imm;
                break;
            case CMD_SUBI:
                t.context.reg[inst.dst_index] = t.context.reg[inst.src1_index] - inst.src2_index_imm;
                break;
            case CMD_LOAD: {
                // dst <- Mem[src1 + src2]
                int addr = t.context.reg[inst.src1_index];
                if (inst.isSrc2Imm) addr += inst.src2_index_imm;
                else addr += t.context.reg[inst.src2_index_imm];
                
                int32_t val;
                SIM_MemDataRead(addr, &val);
                t.context.reg[inst.dst_index] = val;
                
                t.status = WAITING;
                t.wait_cycles = load_latency; 
                break;
            }
            case CMD_STORE: {
                // Mem[dst + src2] <- src1
                int addr = t.context.reg[inst.dst_index];
                if (inst.isSrc2Imm) addr += inst.src2_index_imm;
                else addr += t.context.reg[inst.src2_index_imm];

                int32_t val = t.context.reg[inst.src1_index];
                SIM_MemDataWrite(addr, val);

                t.status = WAITING;
                t.wait_cycles = store_latency;
                break;
            }
            case CMD_HALT:
                t.status = HALTED;
                break;
            case CMD_NOP:
                break;
        }
        total_instructions++;
    }

    void run_blocked_mt() {
        int current_tid = 0;
        int switch_countdown = 0;

        while (true) {
            // Check for global termination
            bool all_halted = true;
            for (const auto& t : threads) {
                if (t.status != HALTED) {
                    all_halted = false;
                    break;
                }
            }
            if (all_halted) break;

            // Handle Context Switch Overhead (before anything else in this cycle)
            if (switch_countdown > 0) {
                switch_countdown--;
                total_cycles++;
                // Decrement memory wait counters during switch overhead
                for (auto& t : threads) {
                    if (t.status == WAITING) {
                        t.wait_cycles--;
                        if (t.wait_cycles <= 0) {
                            t.status = READY;
                        }
                    }
                }
                continue;
            }

            // Current thread
            Thread* curr = &threads[current_tid];

            if (curr->status == READY) {
                // Execute instruction for current thread
                execute_instruction(*curr);
                total_cycles++;
                // Decrement memory wait counters for OTHER threads during this cycle
                for (auto& t : threads) {
                    if (t.id != current_tid && t.status == WAITING) {
                        t.wait_cycles--;
                        if (t.wait_cycles <= 0) {
                            t.status = READY;
                        }
                    }
                }
            } else {
                // Current thread cannot run (WAITING or HALTED), need to find another
                int next_tid = -1;
                
                // Round Robin - search for next ready thread
                for (int i = 1; i <= num_threads; ++i) {
                    int candidate = (current_tid + i) % num_threads;
                    if (threads[candidate].status == READY) {
                        next_tid = candidate;
                        break;
                    }
                }

                if (next_tid != -1) {
                    // Found a ready thread -> Switch with penalty
                    current_tid = next_tid;
                    switch_countdown = switch_overhead;
                    // Don't count this cycle - switch overhead will be counted next
                    // But we still need to decrement wait counters
                    if (switch_countdown == 0) {
                        // No switch overhead - execute immediately
                        execute_instruction(threads[current_tid]);
                        total_cycles++;
                        for (auto& t : threads) {
                            if (t.id != current_tid && t.status == WAITING) {
                                t.wait_cycles--;
                                if (t.wait_cycles <= 0) {
                                    t.status = READY;
                                }
                            }
                        }
                    }
                } else {
                    // No one is ready -> Idle cycle
                    total_cycles++;
                    // Decrement memory wait counters during idle
                    for (auto& t : threads) {
                        if (t.status == WAITING) {
                            t.wait_cycles--;
                            if (t.wait_cycles <= 0) {
                                t.status = READY;
                            }
                        }
                    }
                }
            }
        }
    }

    void run_fine_grained_mt() {
        int current_tid = 0;  // Next thread to try in RR order
        
        while (true) {
            // Check termination
            bool all_halted = true;
            for (const auto& t : threads) {
                if (t.status != HALTED) {
                    all_halted = false;
                    break;
                }
            }
            if (all_halted) break;

            // Round Robin - find next ready thread starting from current_tid
            bool found_thread = false;
            int executed_tid = -1;
            for (int i = 0; i < num_threads; ++i) {
                int candidate = (current_tid + i) % num_threads;
                if (threads[candidate].status == READY) {
                    executed_tid = candidate;
                    execute_instruction(threads[candidate]);
                    total_cycles++;
                    found_thread = true;
                    break;
                }
            }

            if (!found_thread) {
                // No thread ready - idle cycle
                total_cycles++;
            }

            // Decrement wait counters for all WAITING threads that were ALREADY waiting
            // (threads that just started waiting in this cycle should NOT be decremented)
            for (auto& t : threads) {
                // Skip the thread that just executed (it just became WAITING this cycle)
                if (found_thread && t.id == executed_tid) continue;
                
                if (t.status == WAITING) {
                    t.wait_cycles--;
                    if (t.wait_cycles <= 0) {
                        t.status = READY;
                    }
                }
            }

            // Advance RR pointer for next cycle
            // In fine-grained, we move to the next thread only after executing
            if (found_thread) {
                current_tid = (executed_tid + 1) % num_threads;
            }
            // On idle cycle, keep current_tid the same - wait for threads to become ready
        }
    }
};

CoreSimulator* blocked_sim = nullptr;
CoreSimulator* finegrained_sim = nullptr;

void CORE_BlockedMT() {
    if (blocked_sim) delete blocked_sim;
    blocked_sim = new CoreSimulator();
    blocked_sim->run_blocked_mt();
}

void CORE_FinegrainedMT() {
    if (finegrained_sim) delete finegrained_sim;
    finegrained_sim = new CoreSimulator();
    finegrained_sim->run_fine_grained_mt();
}

double CORE_BlockedMT_CPI() {
    if (!blocked_sim) return 0;
    return blocked_sim->get_cpi();
}

double CORE_FinegrainedMT_CPI() {
    if (!finegrained_sim) return 0;
    return finegrained_sim->get_cpi();
}

void CORE_BlockedMT_CTX(tcontext* context, int threadid) {
    if (blocked_sim) {
        blocked_sim->get_thread_context(context, threadid);
    }
}

void CORE_FinegrainedMT_CTX(tcontext* context, int threadid) {
    if (finegrained_sim) {
        finegrained_sim->get_thread_context(context, threadid);
    }
}