#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <ctime>
#include <iomanip>

using namespace std;

// Global random engine
mt19937_64 rng; // 64-bit Mersenne Twister

// Helper to write a single line
void write_access(ofstream& file, char type, unsigned long long addr) {
    file << type << " " << hex << addr << "\n"; // Fast I/O
}

void generate_streaming(ofstream& file, int n) {
    unsigned long long base_addr = 0x10000000;
    uniform_int_distribution<int> percentDist(0, 99);
    cout << "  Generating Streaming (" << n << " ops)..." << endl;
    for (int i = 0; i < n; i++) {
        char type = (percentDist(rng) < 80) ? 'R' : 'W';
        unsigned long long addr = base_addr + ((unsigned long long)i * 64);
        write_access(file, type, addr);
        if (i % 5000000 == 0) cout << "    Progress: " << i << "/" << n << "\r" << flush;
    }
    cout << "    Done." << endl;
}

void generate_random(ofstream& file, int n) {
    uniform_int_distribution<unsigned long long> addrDist(0, 0x10000000);
    uniform_int_distribution<int> percentDist(0, 99);
    cout << "  Generating Random (" << n << " ops)..." << endl;
    for (int i = 0; i < n; i++) {
        char type = (percentDist(rng) < 60) ? 'R' : 'W';
        unsigned long long addr = addrDist(rng); 
        addr = addr & ~0x3F;
        write_access(file, type, addr);
        if (i % 5000000 == 0) cout << "    Progress: " << i << "/" << n << "\r" << flush;
    }
    cout << "    Done." << endl;
}

void generate_gaming(ofstream& file, int n) {
    unsigned long long code_base = 0x400000;
    unsigned long long stack_base = 0x7FFF0000;
    unsigned long long heap_base = 0x10000000;
    unsigned long long current_pc = code_base;
// Distributions
    uniform_int_distribution<int> percentDist(0, 99);
    uniform_int_distribution<int> stackDist(0, 1023); 
    uniform_int_distribution<int> objDist(0, 4999);
    uniform_int_distribution<unsigned long long> jumpDist(0, 0x3FFFFF);

    cout << "  Generating Gaming (" << n << " ops)..." << endl;
    for (int i = 0; i < n; i++) {
        int r = percentDist(rng);
        if (r < 50) { 
            write_access(file, 'R', current_pc);
            current_pc += 4;
            if (percentDist(rng) < 5) {
                current_pc = code_base + jumpDist(rng);
                current_pc = current_pc & ~0x3;
            }
        }
        else if (r < 70) { 
            unsigned long long offset = stackDist(rng) * 4; 
            char type = (percentDist(rng) < 60) ? 'R' : 'W';
            write_access(file, type, stack_base - offset);
        }
        else { 
            unsigned long long obj_index = objDist(rng);
            unsigned long long obj_addr = heap_base + (obj_index * 256);
            write_access(file, 'R', obj_addr);
            write_access(file, 'R', obj_addr + 4);
            write_access(file, 'W', obj_addr + 16);
            i += 2;
        }
        if (i % 5000000 == 0) cout << "    Progress: " << i << "/" << n << "\r" << flush;
    }
    cout << "    Done." << endl;
}

void generate_looping(ofstream& file, int n) {
    unsigned long long core_base = 0x50000000;
    int core_size = 512; 
    unsigned long long noise_base = 0x60000000;
    int noise_size = 2048;
    uniform_int_distribution<int> percentDist(0, 99);
    
    cout << "  Generating Looping (" << n << " ops)..." << endl;
    for (int i = 0; i < n; i++) {
        if (percentDist(rng) < 90) {
             int idx;
             if (percentDist(rng) < 80) idx = i % 10; 
             else idx = i % core_size;                
             unsigned long long addr = core_base + (idx * 64);
             write_access(file, 'R', addr);
        } 
        else {
             int idx = i % noise_size;
             unsigned long long addr = noise_base + (idx * 64);
             write_access(file, 'R', addr);
        }
        if (i % 5000000 == 0) cout << "    Progress: " << i << "/" << n << "\r" << flush;
    }
    cout << "    Done." << endl;
}

void generate_thrashing(ofstream& file, int n) {
    int loop_count = 600; 
    unsigned long long base_addr = 0x80000000;
    cout << "  Generating Thrashing (" << n << " ops)..." << endl;
    for (int i = 0; i < n; i++) {
        int idx = i % loop_count;
        unsigned long long addr = base_addr + (idx * 64);
        write_access(file, 'R', addr);
        if (i % 5000000 == 0) cout << "    Progress: " << i << "/" << n << "\r" << flush;
    }
    cout << "    Done." << endl;
}

int main() {
    rng.seed(time(0));

    // Toplam 50 Milyon (Her dosya icin 50 Milyon)
    // Toplam 100 Bin (Her dosya icin 100 Bin)
    int count = 100000; 

    cout << "Trace verileri uretiliyor (Her dosya 100 Bin Erisim)..." << endl;
    
    {
        ofstream f("streaming.trace");
        if(f.is_open()) { generate_streaming(f, count); f.close(); }
    }
    {
        ofstream f("random.trace");
        if(f.is_open()) { generate_random(f, count); f.close(); }
    }
    {
        ofstream f("gaming.trace");
        if(f.is_open()) { generate_gaming(f, count); f.close(); }
    }
    {
        ofstream f("looping.trace");
        if(f.is_open()) { generate_looping(f, count); f.close(); }
    }
    {
        ofstream f("thrashing.trace");
        if(f.is_open()) { generate_thrashing(f, count); f.close(); }
    }

    cout << "\nISLEM TAMAMLANDI." << endl;
    cout << "Olusturulan Dosyalar: streaming, random, gaming, looping, thrashing (Her biri 100k ops)" << endl;

    return 0;
}
