#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstdint>
#include <cstdio>

using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;
using std::vector;

struct Evicted {
    bool valid = false;
    bool dirty = false;
    uint64_t addr = 0;
};
typedef struct {
    bool valid = false;
    bool dirty = false;
    uint64_t tag = 0;
    uint64_t lru = 0;
} Line;

int log2_int(int num) {
    int count =0 ;
    while (num > 1){
        count +=1 ;
        num /=2;
    }
    return count ;
}
typedef struct{
    int num_sets;
    int ways;
    int set_bits;
    int offset_bits;
    vector <vector<Line>> table;
    uint64_t timer ;
}cache;
//declaration :
unsigned get_set(const cache& c, uint64_t addr);
uint64_t get_tag(const cache& c, uint64_t addr);
int find_way(cache& c, uint64_t addr);
bool access_hit_update(cache& c, uint64_t addr, bool isWrite);
int choose_victim(cache& c, unsigned set);
Evicted install_line(cache& c, uint64_t addr, bool dirty);
uint64_t build_block_addr(const cache& c, uint64_t tag, unsigned set);
bool invalidate_line(cache& c, uint64_t addr, bool* was_dirty = nullptr);

//implementation :
unsigned get_set(const cache& c, uint64_t addr){
    return (addr >> c.offset_bits) & (c.num_sets - 1);
}
uint64_t get_tag(const cache& c, uint64_t addr){
    return addr >> (c.offset_bits + c.set_bits);
}
int find_way(cache& c, uint64_t addr){
    unsigned set= get_set(c,addr);
    uint64_t tag = get_tag(c,addr);
    for (unsigned i = 0; i < c.ways; ++i) {
        if ((c.table[set][i].valid) && (c.table[set][i].tag == tag)) {
            return (int)i; // HIT
        }
    }
    return -1; // MISS
}
bool access_hit_update(cache& c, uint64_t addr, bool is_write){
    unsigned set = get_set(c, addr);
    int hit_or_miss = find_way(c,addr);
    if (hit_or_miss < 0) return false ;
    c.timer ++;
    c.table[set][hit_or_miss].lru = c.timer;
    if (is_write) {
        c.table[set][hit_or_miss].dirty = true;
    }
    return true;
}
int choose_victim(cache& c, unsigned set){
    for (unsigned i = 0; i < c.ways; ++i){
        if( !(c.table[set][i].valid))return i;
    }
   uint64_t min_lru = c.table[set][0].lru;
    int victim =0 ;
    for (unsigned i = 1; i < c.ways; ++i){
        if( c.table[set][i].lru < min_lru){
            min_lru = c.table[set][i].lru;
            victim = i ;
        }
    }
    return victim;
}
Evicted install_line(cache& c, uint64_t addr, bool dirty) {
    unsigned set = get_set(c, addr);
    uint64_t tag = get_tag(c, addr);
    int victim = choose_victim(c, set);

    Evicted ev;
    if (c.table[set][victim].valid) {
        ev.valid = true;
        ev.dirty = c.table[set][victim].dirty;
        ev.addr  = build_block_addr(c, c.table[set][victim].tag, set);
    }

    c.table[set][victim].valid = true;
    c.table[set][victim].dirty = dirty;
    c.table[set][victim].tag   = tag;

    ++c.timer;
    c.table[set][victim].lru = c.timer;

    return ev;
}

uint64_t build_block_addr(const cache& c, uint64_t tag, unsigned set) {
    uint64_t x = (tag << c.set_bits) | (uint64_t)set;
    return x << c.offset_bits;
}
bool invalidate_line(cache& c, uint64_t addr, bool* was_dirty ) {
    unsigned set = get_set(c, addr);
    uint64_t tag = get_tag(c, addr);
    for (unsigned i = 0; i < (unsigned)c.ways; i++) {
        if (c.table[set][i].valid && c.table[set][i].tag == tag) {
            if (was_dirty) *was_dirty = c.table[set][i].dirty;
            c.table[set][i].valid = false;
            c.table[set][i].dirty = false;
            return true;
        }
    }
    return false;
}
int main(int argc, char **argv) {

	if (argc < 20) {
		cerr << "Not enough arguments" << endl;
		return 0;
	}

	// Get input arguments

	// File
	// Assuming it is the first argument
	char* fileString = argv[1];
	ifstream file(fileString); //input file stream
	string line;
	if (!file || !file.good()) {
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}

	unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

	for (int i = 2; i < 19; i += 2) {
		string s(argv[i]);
		if (s == "--mem-cyc") {
			MemCyc = atoi(argv[i + 1]);
		} else if (s == "--bsize") {
			BSize = atoi(argv[i + 1]);
		} else if (s == "--l1-size") {
			L1Size = atoi(argv[i + 1]);
		} else if (s == "--l2-size") {
			L2Size = atoi(argv[i + 1]);
		} else if (s == "--l1-cyc") {
			L1Cyc = atoi(argv[i + 1]);
		} else if (s == "--l2-cyc") {
			L2Cyc = atoi(argv[i + 1]);
		} else if (s == "--l1-assoc") {
			L1Assoc = atoi(argv[i + 1]);
		} else if (s == "--l2-assoc") {
			L2Assoc = atoi(argv[i + 1]);
		} else if (s == "--wr-alloc") {
			WrAlloc = atoi(argv[i + 1]);
		} else {
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}
    cache L1, L2;
    L1.set_bits= L1Size - BSize - L1Assoc;
    L1.ways = 1<< L1Assoc;
    L1.num_sets = 1<< L1.set_bits;
    L1.offset_bits = BSize;
    L1.timer = 0 ;
    L1.table.assign(L1.num_sets, vector<Line>(L1.ways));

    L2.set_bits = L2Size - BSize - L2Assoc;
    L2.ways = 1<< L2Assoc;
    L2.num_sets = 1<< L2.set_bits;
    L2.offset_bits = BSize;
    L2.timer = 0 ;
    L2.table.assign(L2.num_sets, vector<Line>(L2.ways));

    double total_time = 0.0;
    uint64_t L1Accesses = 0;
    uint64_t L1Misses = 0;
    uint64_t L2Accesses = 0;
    uint64_t L2Misses = 0;
    while (getline(file, line)) {

        stringstream ss(line);
        string address;
        char operation = 0; // read (R) or write (W)
        if (!(ss >> operation >> address)) {
            // Operation appears in an Invalid format
            cout << "Command Format error" << endl;
            return 0;
        }
        string cutAddress = address.substr(2);
        uint64_t addr = strtoull(cutAddress.c_str(), NULL, 16);
        bool isWrite = (operation == 'w' || operation == 'W');
        L1Accesses++;
        bool l1hit = access_hit_update(L1, addr, isWrite);
        if (l1hit) {
            total_time += L1Cyc;
            continue;
        }

        // L1 miss
        L1Misses++;
        total_time += L1Cyc;

        ++L2Accesses;
        total_time += L2Cyc;
        if (WrAlloc == 0 && isWrite) {
            bool l2hit = access_hit_update(L2, addr, true);
            if (!l2hit) {
                L2Misses++;
                total_time += MemCyc; // write to memory
            }
            continue;
        }

        bool l2hit = access_hit_update(L2, addr, false);
        if (!l2hit) {
            L2Misses++;
            total_time += MemCyc;
            Evicted ev2 = install_line(L2, addr, false);
            if (ev2.valid) {
                bool l1_dirty = false;
                invalidate_line(L1, ev2.addr, &l1_dirty);
            }
        }
        Evicted ev1 = install_line(L1, addr, isWrite);

        if (ev1.valid && ev1.dirty) {
            bool wb_hit = access_hit_update(L2, ev1.addr, true);

            if (!wb_hit) {
                Evicted ev2 = install_line(L2, ev1.addr, true);
                if (ev2.valid) {
                    bool l1_dirty = false;
                    invalidate_line(L1, ev2.addr, &l1_dirty);
                }
            }
        }

    }

    double L1MissRate = (L1Accesses ? (double)L1Misses / L1Accesses : 0.0);
    double L2MissRate = (L2Accesses ? (double)L2Misses / L2Accesses : 0.0);
    double avgAccTime = (L1Accesses ? total_time / L1Accesses : 0.0);

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}
