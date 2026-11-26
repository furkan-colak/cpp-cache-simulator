#ifndef CACHE_SIM_H
#define CACHE_SIM_H
#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <string>
#include <iomanip>
#include <algorithm>
#include <map>
#include <ctime>
#include <windows.h> 
#include <sstream>
// ==========================================
// TİP TANIMLAMALARI VE SABİTLER
// ==========================================
enum AccessType { LOAD = 'R', STORE = 'W' };
#define COLOR_DEFAULT 7
#define COLOR_GREEN 10
#define COLOR_RED 12
#define COLOR_YELLOW 14
#define COLOR_CYAN 11
#define COLOR_WHITE 15
inline void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}
// ==========================================
// CONFIG YAPISI (Dinamik Yapılandırma)
// ==========================================
struct Config {
    // --- L1 Cache Ayarlari ---
    long long L1_SIZE = 32768;     // Boyut (Byte) [Default: 32KB]
    int L1_ASSOC = 4;              // Yolluluk (Associativity)
    int L1_LATENCY = 4;            // Erisim Gecikmesi (Cycle)

    // --- L2 Cache Ayarlari ---
    long long L2_SIZE = 262144;    // [Default: 256KB]
    int L2_ASSOC = 8;
    int L2_LATENCY = 12;

    // --- L3 Cache Ayarlari ---
    long long L3_SIZE = 2097152;   // [Default: 2MB]
    int L3_ASSOC = 16;
    int L3_LATENCY = 40;

    // --- Bellek ve Sistem Ayarlari ---
    int RAM_LATENCY = 200;         // Ana Bellek Gecikmesi
    int BLOCK_SIZE = 64;           // Cache Satir Boyutu (Byte)

    // --- Ozel Moduller ---
    bool DMA_ENABLED = false;      // DMA (Direct Memory Access) Modu
    int DMA_CHANCE_PERCENT = 0;    // DMA Tetiklenme Olasiligi (%)
    int DMA_PENALTY = 50;          // DMA Gecikme Cezasi

    int INTERRUPT_FREQ = 0;        // Kesme (Interrupt) Sikligi (Instruction)
    int INTERRUPT_PENALTY = 200;   // Kesme Cezasi (Cycle)

    bool PREFETCHER_ENABLED = false; // On-Getirici (Next-Line Prefetcher)

    // --- Algoritma Secimi ---
    std::string ALGORITHM = "LRU"; // Degistirme Politikasi (LRU, FIFO, vb.)
    std::string CURRENT_TRACE_FILE = "unknown.trace"; // Raporlama icin dosya adi
};

// Global Config
inline Config g_Config;

const std::vector<std::string> KNOWN_ALGORITHMS = {
    "LRU", "RANDOM", "FIFO", "LFU", "PLRU"//Bulunan algoritmalar
};

// ==========================================
// YARDIMCI FONKSİYONLAR
// ==========================================
inline bool loadConfig(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[HATA] Config dosyasi (" << filename << ") bulunamadi!" << std::endl;
        return false;
    }

    std::string line;
    std::map<std::string, int*> intParams = {
        {"L1_ASSOCIATIVITY", &g_Config.L1_ASSOC},       // L1 Yolluluk
        {"L1_LATENCY", &g_Config.L1_LATENCY},           // L1 Gecikme
        {"L2_ASSOCIATIVITY", &g_Config.L2_ASSOC},       // L2 Yolluluk
        {"L2_LATENCY", &g_Config.L2_LATENCY},           // L2 Gecikme
        {"L3_ASSOCIATIVITY", &g_Config.L3_ASSOC},       // L3 Yolluluk
        {"L3_LATENCY", &g_Config.L3_LATENCY},           // L3 Gecikme
        {"RAM_LATENCY", &g_Config.RAM_LATENCY},         // RAM Gecikme
        {"BLOCK_SIZE", &g_Config.BLOCK_SIZE},           // Blok Boyutu
        {"DMA_CHANCE_PERCENT", &g_Config.DMA_CHANCE_PERCENT}, // DMA Sansi
        {"DMA_PENALTY", &g_Config.DMA_PENALTY},         // DMA Cezasi
        {"INTERRUPT_FREQ", &g_Config.INTERRUPT_FREQ},   // Kesme Sikligi
        {"INTERRUPT_PENALTY", &g_Config.INTERRUPT_PENALTY} // Kesme Cezasi
    };
    
    std::map<std::string, long long*> longParams = {
        {"L1_SIZE", &g_Config.L1_SIZE}, // L1 Boyutu
        {"L2_SIZE", &g_Config.L2_SIZE}, // L2 Boyutu
        {"L3_SIZE", &g_Config.L3_SIZE}  // L3 Boyutu
    };

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string valueStr = line.substr(delimiterPos + 1);
            key.erase(std::remove_if(key.begin(), key.end(), ::isspace), key.end());
            
            if (key == "ALGORITHM") {
                valueStr.erase(std::remove_if(valueStr.begin(), valueStr.end(), ::isspace), valueStr.end());
                g_Config.ALGORITHM = valueStr;
                continue;
            }
            if (key == "DMA_ENABLED") {
                 g_Config.DMA_ENABLED = (valueStr.find("1") != std::string::npos || valueStr.find("true") != std::string::npos);
                 continue;
            }   
            if (key == "PREFETCHER_ENABLED") {
                 g_Config.PREFETCHER_ENABLED = (valueStr.find("1") != std::string::npos || valueStr.find("true") != std::string::npos);
                 continue;
            }

            if (intParams.find(key) != intParams.end()) {
                try { *intParams[key] = std::stoi(valueStr); } catch (...) {}
            }
            else if (longParams.find(key) != longParams.end()) {
                 try { *longParams[key] = std::stoll(valueStr); } catch (...) {}
            }
        }
    }
    file.close();

    long long totalCacheBytes = g_Config.L1_SIZE + g_Config.L2_SIZE + g_Config.L3_SIZE;
    std::cout << "[BILGI] Konfigurasyon yuklendi: " << filename << std::endl;
    std::cout << "[BILGI] Toplam Cache Boyutu (Mapped): " << (totalCacheBytes/1024/1024) << " MB" << std::endl;
    // Detayli bilgi main'de de yazdirilabilir
    return true;
}

struct CacheBlock {
    unsigned long long tag = 0;      // Etiket (Adresin ust kismi)
    long long lastAccessTime = 0;    // Son Erisim Zamani (LRU icin)
    long long insertionTime = 0;     // Ekleme Zamani (FIFO icin)
    long long accessCount = 0;       // Erisim Sayisi (LFU icin)
    bool valid = false;              // Gecerlilik Biti (Doluluk)
    bool dirty = false;              // Kirlilik Biti (Degisiklik var mi?)
};

inline bool isPowerOfTwo(unsigned long long x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}

// ... CacheLevel class ...
class CacheLevel {
public: // Testing icin public yapiyoruz
    std::string name;             // Cache Adi (L1, L2...)
    unsigned long long size;      // Toplam Boyut (Byte)
    int blockSize;                // Blok Boyutu
    int associativity;            // Yol Sayisi
    unsigned long long numSets;   // Set Sayisi
    int latency;                  // Gecikme

    // --- Adres Cozme Parametreleri ---
    int offsetBits;
    int indexBits;
    unsigned long long indexMask;

    // --- Memory Mapping Degiskenleri ---
    std::string mapFileName;      // Gecici Dosya Adi
    HANDLE hFile = INVALID_HANDLE_VALUE; // Dosya Taniticisi
    HANDLE hMap = INVALID_HANDLE_VALUE;  // Map Taniticisi
    CacheBlock* data = nullptr;          // Bellek Pointer'i (Veriye Erisim)
    
    // --- Algoritma Verileri ---
    std::vector<unsigned int> treeBits;  // PLRU Agac Bitleri

    // --- Istatistikler ---
    long long hits = 0;
    long long misses = 0;
    long long evictions = 0;

    long long readHits = 0;
    long long writeHits = 0;
    long long readMisses = 0;
    long long writeMisses = 0;

    friend class MemorySystem; 

public:
    CacheLevel(std::string n, unsigned long long s, int b, int a, int lat)
        : name(n), size(s), blockSize(b), associativity(a), latency(lat) {

        if (blockSize == 0 || associativity == 0) exit(1);
        numSets = size / ((unsigned long long)blockSize * associativity);
        if (!isPowerOfTwo(numSets)) exit(1);

        offsetBits = (int)log2((double)blockSize);
        indexBits = (int)log2((double)numSets);
        indexMask = (unsigned long long)(numSets - 1);

        // Unique Mapping Name
        mapFileName = "MMap_" + name + "_" + std::to_string(rand()) + ".tmp";
        DeleteFileA(mapFileName.c_str());

        hFile = CreateFileA(mapFileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (hFile == INVALID_HANDLE_VALUE) exit(1);

        unsigned long long totalBytes = numSets * associativity * sizeof(CacheBlock);
        LARGE_INTEGER liSize; liSize.QuadPart = totalBytes;

        hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, liSize.HighPart, liSize.LowPart, NULL);
        if (hMap == NULL) { CloseHandle(hFile); exit(1); }

        data = (CacheBlock*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, totalBytes);
        if (data == nullptr) { CloseHandle(hMap); CloseHandle(hFile); exit(1); }

        treeBits.resize(numSets, 0); 
    }

    ~CacheLevel() {
        if (data) UnmapViewOfFile(data);
        if (hMap) CloseHandle(hMap);
        if (hFile) CloseHandle(hFile); 
    }

    // COPY PROTECTION (Rule of Three Violation Fix)
    // Bu sinif raw handle'lar yonettigi icin kopyalanmamalidir.
    // Aksi takdirde destructor cagirildiginda "double free" hatasi olusur.
    CacheLevel(const CacheLevel&) = delete;
    CacheLevel& operator=(const CacheLevel&) = delete;

    void updatePlru(unsigned long long setIdx, int wayIdx) {
        unsigned int state = treeBits[setIdx];
        int leaf = wayIdx + associativity;
        while (leaf > 1) {
             int parent = leaf / 2;
             int bitIdx = parent - 1; 
             int nextVal = 1 - (leaf % 2);
             if (nextVal) state |= (1 << bitIdx);
             else state &= ~(1 << bitIdx);
             leaf = parent;
        }
        treeBits[setIdx] = state;
    }

    int getPlruVictim(unsigned long long setIdx) {//PLRU algoritmasi icin
        unsigned int state = treeBits[setIdx];//setin plru agaci
        int idx = 1; //agac basi
        while (idx < associativity) {//agac sonuna kadar
             int bit = (state >> (idx - 1)) & 1;//agac bitlerini oku
             if (bit == 0) idx = idx * 2;       
             else idx = idx * 2 + 1;            
        }
        return idx - associativity;
    }

    bool accessAndUpdate(unsigned long long address, char type, long long globalTime) {
        // 1. ADRES AYRISTIRMA
        unsigned long long index = (address >> offsetBits) & indexMask;//set indexini bul
        unsigned long long tag = address >> (offsetBits + indexBits);//etiketi bul
        unsigned long long baseIdx = index * associativity;//setin basini bul

        // 2. SET ICINDE ARAMA (Associative Search)
        for (int i = 0; i < associativity; i++) {
            CacheBlock& blk = data[baseIdx + i]; //setin icindeki bloklari oku

            // HIT KONTROLU: Dolu mu? VE Etiket tutuyor mu?
            if (blk.valid && blk.tag == tag) {
                // --- HIT (ISABET) ---
                hits++;
                if (type == LOAD) readHits++; else writeHits++;
                
                // Algoritma Parametrelerini Guncelle
                blk.lastAccessTime = globalTime; // LRU icin
                blk.accessCount++;               // LFU icin
                if (type == STORE) blk.dirty = true; // Yazma ise Dirty yap
                
                if (g_Config.ALGORITHM == "PLRU") updatePlru(index, i);
                return true;
            }
        }

        // --- MISS (ISKALAMA) ---
        misses++;
        if (type == LOAD) readMisses++; else writeMisses++;
        return false;
    }

    void install(unsigned long long address, char type, long long globalTime,
        bool& evicted, unsigned long long& evictedAddress, bool& evictedDirty) {

        unsigned long long index = (address >> offsetBits) & indexMask;
        unsigned long long tag = address >> (offsetBits + indexBits);
        unsigned long long baseIdx = index * associativity;

        // --- KURBAN SECIMI (VICTIM SELECTION) ---
        int emptyWay = -1; // Bos yer var mi?
        
        // Algoritmalar icin adaylar
        int lruVictim = -1; long long minLruTime = -1;   // En eski erisilen (LRU)
        int fifoVictim = -1; long long minFifoTime = -1; // En eski eklenen (FIFO)
        int lfuVictim = -1; long long minLfuCount = -1;  // En az kullanilan (LFU)

        // Set icindeki tum yollari tara
        for (int i = 0; i < associativity; i++) {
            const CacheBlock& blk = data[baseIdx + i];
            
            // Oncelikle bos yer varsa onu tut, gerisine bakmaya gerek kalmayabilir
            if (!blk.valid) { emptyWay = i; break; }
            
            // Doluysa algoritmalar icin en kotu adayi bul
            if (minLruTime == -1 || blk.lastAccessTime < minLruTime) { 
                minLruTime = blk.lastAccessTime; lruVictim = i; 
            }
            if (minFifoTime == -1 || blk.insertionTime < minFifoTime) { 
                minFifoTime = blk.insertionTime; fifoVictim = i; 
            }
            if (minLfuCount == -1 || blk.accessCount < minLfuCount) { 
                minLfuCount = blk.accessCount; lfuVictim = i; 
            }
        }

        int targetWay = -1;
        // Eger bos yer varsa algoritmaya bakma, direk oraya koy
        if (emptyWay != -1) targetWay = emptyWay;
        else {
            // Hepsi dolu, birini atmamiz lazim (Eviction)
            if (g_Config.ALGORITHM == "RANDOM") targetWay = rand() % associativity;
            else if (g_Config.ALGORITHM == "FIFO") targetWay = fifoVictim;
            else if (g_Config.ALGORITHM == "LFU") targetWay = lfuVictim;
            else if (g_Config.ALGORITHM == "PLRU") targetWay = getPlruVictim(index);
            else targetWay = lruVictim; // Varsayilan: LRU
        }
        
        CacheBlock& targetBlock = data[baseIdx + targetWay];
        evicted = false;
        if (emptyWay == -1) {
            evictions++;
            evicted = true;
            evictedAddress = (targetBlock.tag << (indexBits + offsetBits)) | (index << offsetBits);
            evictedDirty = targetBlock.dirty;
        }

        targetBlock.valid = true;
        targetBlock.tag = tag;
        targetBlock.lastAccessTime = globalTime;
        targetBlock.insertionTime = globalTime; 
        targetBlock.accessCount = 1;            
        targetBlock.dirty = (type == STORE);
        
        if (g_Config.ALGORITHM == "PLRU") updatePlru(index, targetWay);
    }

    void writeBack(unsigned long long address, long long globalTime, bool &evicted, unsigned long long &evAddr, bool &evDirty) {
        unsigned long long index = (address >> offsetBits) & indexMask;
        unsigned long long tag = address >> (offsetBits + indexBits);
        unsigned long long baseIdx = index * associativity;

        for (int i = 0; i < associativity; i++) {
            CacheBlock& blk = data[baseIdx + i];
            if (blk.valid && blk.tag == tag) {
                blk.dirty = true;
                blk.lastAccessTime = globalTime;
                evicted = false;
                return;
            }
        }
        install(address, STORE, globalTime, evicted, evAddr, evDirty);
    }

    void invalidate(unsigned long long address) {
        unsigned long long index = (address >> offsetBits) & indexMask;
        unsigned long long tag = address >> (offsetBits + indexBits);
        unsigned long long baseIdx = index * associativity;
        for (int i = 0; i < associativity; i++) {
             CacheBlock& blk = data[baseIdx + i];
             if (blk.valid && blk.tag == tag) { blk.valid = false; return; }
        }
    }
};

class MemorySystem {
public: // Testing icin Public
    // --- Cache Katmanlari ---
    CacheLevel L1;
    CacheLevel L2;
    CacheLevel L3;

    // --- Global Istatistikler ---
    long long ramAccesses = 0;       // RAM'e dusen istek sayisi
    long long globalTime = 0;        // Simuilasyon Saati (Global Clock)
    
    // --- Performans Sayaclari ---
    long long totalSimulatedCycles = 0; // Toplam Gecen Sure (Cycle)
    long long dmaStallCycles = 0;       // DMA Bekleme Suresi
    long long interruptCycles = 0;      // Kesme Bekleme Suresi
    long long totalInstructions = 0;    // Toplam Islenen Komut
    
    // --- Ozel Durumlar ---
    long long droppedFrames = 0;        // DMA Kayip Frame Sayisi
    double currentBufferLoad = 0.0;     // Anlik Buffer Dolulugu

    std::vector<int> sampledLatencies;  // P95 Hesaplamasi icin orneklemler

    bool checkProbe(CacheLevel& cache, unsigned long long address) {
        unsigned long long index = (address >> cache.offsetBits) & cache.indexMask;
        unsigned long long tag = address >> (cache.offsetBits + cache.indexBits);
        unsigned long long baseIdx = index * cache.associativity;
        for (int i = 0; i < cache.associativity; i++) {
            if (cache.data[baseIdx + i].valid && cache.data[baseIdx + i].tag == tag) return true;
        }
        return false;
    }

    void handlePrefetch(unsigned long long address) {
        if (checkProbe(L1, address)) return; 
        if (checkProbe(L2, address)) { handleInstallL1(address, LOAD); return; }
        if (checkProbe(L3, address)) { handleInstallL2(address, LOAD); handleInstallL1(address, LOAD); return; }
        ramAccesses++; 
        handleInstallL3(address, LOAD);
        handleInstallL2(address, LOAD);
        handleInstallL1(address, LOAD);
    }

public:
    MemorySystem()
        : L1("L1 Cache", g_Config.L1_SIZE, g_Config.BLOCK_SIZE, g_Config.L1_ASSOC, g_Config.L1_LATENCY),
          L2("L2 Cache", g_Config.L2_SIZE, g_Config.BLOCK_SIZE, g_Config.L2_ASSOC, g_Config.L2_LATENCY),
          L3("L3 Cache", g_Config.L3_SIZE, g_Config.BLOCK_SIZE, g_Config.L3_ASSOC, g_Config.L3_LATENCY)
    {
        sampledLatencies.reserve(100000); 
    }

    void processTrace(unsigned long long address, char type) {
        totalInstructions++;
        globalTime++; 
        long long currentAccessLatency = 0;
        
        bool l1Hit = L1.accessAndUpdate(address, type, globalTime);
        if (l1Hit) {
            currentAccessLatency = g_Config.L1_LATENCY;
        } else {
            bool l2Hit = L2.accessAndUpdate(address, type, globalTime);
            if (l2Hit) {
                currentAccessLatency = g_Config.L1_LATENCY + g_Config.L2_LATENCY;
                handleInstallL1(address, type);
            } else {
                bool l3Hit = L3.accessAndUpdate(address, type, globalTime);
                if (l3Hit) {
                    currentAccessLatency = g_Config.L1_LATENCY + g_Config.L2_LATENCY + g_Config.L3_LATENCY;
                    handleInstallL2(address, type);
                    handleInstallL1(address, type);
                } else {
                    ramAccesses++;
                    currentAccessLatency = g_Config.L1_LATENCY + g_Config.L2_LATENCY + g_Config.L3_LATENCY + g_Config.RAM_LATENCY;
                    handleInstallL3(address, LOAD);
                    handleInstallL2(address, LOAD);
                    handleInstallL1(address, type);
                }
            //////////////////////
            }// prefetch
            //////////////////////
            if (g_Config.PREFETCHER_ENABLED && type == LOAD) {
                unsigned long long nextAddr = address + g_Config.BLOCK_SIZE;
                handlePrefetch(nextAddr);
            }
        }

        if (g_Config.DMA_ENABLED) {
            int loadChange = (rand() % 10) - 4; 
            currentBufferLoad += loadChange;
            if (currentBufferLoad < 0) currentBufferLoad = 0;
            if (currentBufferLoad > 100) currentBufferLoad = 100;
            if ((rand() % 100) < g_Config.DMA_CHANCE_PERCENT) {
                currentAccessLatency += g_Config.DMA_PENALTY;
                dmaStallCycles += g_Config.DMA_PENALTY;
            }
            if (currentBufferLoad > 95.0 && (rand() % 100) < 20) droppedFrames++;
        }
        //////////////////////////////////
        //kesme
        ///////////////////////////////////
        if (g_Config.INTERRUPT_FREQ > 0 && (totalInstructions % g_Config.INTERRUPT_FREQ == 0)) {
            currentAccessLatency += g_Config.INTERRUPT_PENALTY;
            interruptCycles += g_Config.INTERRUPT_PENALTY;
        }

        totalSimulatedCycles += currentAccessLatency;
        if (totalInstructions % 100 == 0) sampledLatencies.push_back((int)currentAccessLatency);
    }
    
    void handleInstallL1(unsigned long long address, char type) {
        bool evicted = false; unsigned long long evAddr = 0; bool evDirty = false;
        L1.install(address, type, globalTime, evicted, evAddr, evDirty);
        if (evicted && evDirty) {
             bool l2Evict = false; unsigned long long l2Addr = 0; bool l2Dirty = false;
             L2.writeBack(evAddr, globalTime, l2Evict, l2Addr, l2Dirty);
             if (l2Evict) { 
                 L1.invalidate(l2Addr); 
                 if (l2Dirty) {
                     bool l3Evict = false; unsigned long long l3Addr = 0; bool l3Dirty = false;
                     L3.writeBack(l2Addr, globalTime, l3Evict, l3Addr, l3Dirty);
                     if (l3Evict) { 
                        L2.invalidate(l3Addr); L1.invalidate(l3Addr); 
                        if (l3Dirty) ramAccesses++;
                     }
                 }
             }
        }
    }

    void handleInstallL2(unsigned long long address, char type) {
        bool evicted = false; unsigned long long evAddr = 0; bool evDirty = false;
        L2.install(address, type, globalTime, evicted, evAddr, evDirty);
        if (evicted) {
             L1.invalidate(evAddr); 
             if (evDirty) {
                bool l3Evict = false; unsigned long long l3Addr = 0; bool l3Dirty = false;
                L3.writeBack(evAddr, globalTime, l3Evict, l3Addr, l3Dirty);
                if (l3Evict) { L2.invalidate(l3Addr); L1.invalidate(l3Addr); if (l3Dirty) ramAccesses++; }
            }
        }
    }

    void handleInstallL3(unsigned long long address, char type) {
        bool evicted = false; unsigned long long evAddr = 0; bool evDirty = false;
        L3.install(address, type, globalTime, evicted, evAddr, evDirty);
        if (evicted) { L2.invalidate(evAddr); L1.invalidate(evAddr); if (evDirty) ramAccesses++; }
    }

    struct SimStats {
        long long cycles;
        double amat;
        double cpi; 
        double p95_latency;
        double jitter;
        long long dmaCycles;
        long long intCycles;
        long long drops;
        double avgBuffer;
        double l1HitRate; 
        double bandwidth; 
    };

    SimStats getStats() const {
        long long totalAccesses = L1.hits + L1.misses;
        
        double amat = (totalAccesses > 0) ? (double)totalSimulatedCycles / totalAccesses : 0;
        double cpi = (totalInstructions > 0) ? (double)totalSimulatedCycles / totalInstructions : 0;

        double p95 = 0;
        double jitter = 0;

        if (!sampledLatencies.empty()) {
            std::vector<int> sorted = sampledLatencies;
            std::sort(sorted.begin(), sorted.end());
            int idx = (int)(sorted.size() * 0.95);
            if(idx >= sorted.size()) idx = sorted.size() - 1;
            p95 = sorted[idx];
            
            double mean = 0;
            for(int val : sorted) mean += val;
            mean /= sorted.size();
            
            double variance = 0;    
            for(int val : sorted) variance += (val - mean) * (val - mean);
            variance /= sorted.size();
            jitter =  std::sqrt(variance);
        }
        
        double l1Rate = (totalAccesses > 0) ? ((double)L1.hits / totalAccesses * 100.0) : 0.0;
        
        // Bandwidth Calc (Assume 3.0 GHz)
        double seconds = (double)totalSimulatedCycles / 3000000000.0;
        long long totalBytes = ramAccesses * 64; // Block Size = 64
        double bw = 0.0;
        if (seconds > 0) {
            bw = (double)totalBytes / (1024.0 * 1024.0 * 1024.0) / seconds; // GB/s
        }

        return { totalSimulatedCycles, amat, cpi, p95, jitter, dmaStallCycles, interruptCycles, droppedFrames, currentBufferLoad, l1Rate, bw };
    }
    
    // --- Gorsellestirme Yardimcilari ---
    std::string drawProgressBar(double percent, int width = 20) const {
        int bars = (int)((percent / 100.0) * width);
        if (bars > width) bars = width;
        if (bars < 0) bars = 0;
        
        std::string result = "[";
        for (int i = 0; i < width; i++) {
            if (i < bars) result += "#";
            else result += ".";
        }
        result += "] " + std::to_string((int)percent) + "%";
        return result;
    }

    void report(const std::string& csvFilename) const {
        SimStats s = getStats();

        std::cout << std::string(85, '=') << std::endl;
        setColor(COLOR_CYAN);
        std::cout << "              SISTEM SIMULASYON RAPORU (Donanim Etkileri Dahil)" << std::endl;
        setColor(COLOR_DEFAULT);
        std::cout << std::string(85, '-') << std::endl;
        std::cout << "| Cache(Onblk)| Hits(Isabet)| Miss(Iskal) | Rate(Oran) | Visual (Gorsel)             |" << std::endl;
        std::cout << std::string(85, '-') << std::endl;

        auto printLevel = [&](const CacheLevel& c) {
             long long total = c.hits + c.misses;
             double hitRate = (total > 0) ? ((double)c.hits / total) * 100.0 : 0.0;
             
             std::cout << "| " << std::setw(10) << std::left << c.name
                << "| ";
             
             setColor(COLOR_GREEN); std::cout << std::setw(12) << c.hits; setColor(COLOR_DEFAULT);
             std::cout << "| ";
             setColor(COLOR_RED); std::cout << std::setw(12) << c.misses; setColor(COLOR_DEFAULT);
             
             std::cout << "| " << std::setw(10) << std::fixed << std::setprecision(2) << hitRate << "% "
                << "| " << std::setw(28) << drawProgressBar(hitRate, 15) << "|" << std::endl;
        };

        printLevel(L1);
        printLevel(L2);
        printLevel(L3);

        std::cout << std::string(85, '-') << std::endl;
        std::cout << " RAM Erisim Sayisi         : " << ramAccesses << std::endl;
        std::cout << " Toplam Islenen Komut      : " << totalInstructions << std::endl;
        std::cout << std::string(85, '-') << std::endl;
        
        // --- Sistem Karnesi (Basit ASCII Box) ---
        setColor(COLOR_YELLOW);
        std::cout << " SISTEM KARNESI:" << std::endl;
        setColor(COLOR_DEFAULT);
        std::cout << " +--------------------------+--------------------------+" << std::endl;
        std::cout << " |  Metrik                  |  Deger                   |" << std::endl;
        std::cout << " +--------------------------+--------------------------+" << std::endl;
        
        std::cout << " |  Performans Puani (CPI)  |  ";
        if(s.cpi < 2.0) setColor(COLOR_GREEN); else if(s.cpi < 5.0) setColor(COLOR_YELLOW); else setColor(COLOR_RED);
        std::cout << std::setw(24) << std::left << std::fixed << std::setprecision(2) << s.cpi << "|";
        setColor(COLOR_DEFAULT); std::cout << std::endl;

        std::cout << " |  Ortalama Gecikme (AMAT) |  " << s.amat << " cycles" << std::string(17 - std::to_string((int)s.amat).length(), ' ') << "|" << std::endl;
        std::cout << " |  En Kotu %5 (P95)        |  " << s.p95_latency << " cycles" << std::string(17 - std::to_string((int)s.p95_latency).length(), ' ') << "|" << std::endl;
        std::cout << " |  RAM Bandwidth (3GHz)    |  " << s.bandwidth << " GB/s " << std::string((s.bandwidth >= 10 ? 11 : 12) - std::to_string((int)s.bandwidth).length(), ' ') << "|"<< std::endl;
        
        if (s.drops > 15) {
            setColor(COLOR_YELLOW);
            std::cout << " [METRIC] DMA Buffer Drop (Simule Edilen Paket Kaybi): " << s.drops << " Frame dropped." << std::endl;
             std::cout << " [DURUM] *** KRITIK ***" << std::endl;
             setColor(COLOR_DEFAULT);
        } else {
             setColor(COLOR_GREEN);
             std::cout << " [DURUM] Sistem Stabil." << std::endl;
             setColor(COLOR_DEFAULT);
        }
        std::cout << std::string(85, '=') << std::endl;

        // Dosya bos mu kontrol et
        bool fileExistsAndNotEmpty = false;
        {
            std::ifstream rFile(csvFilename);
            if (rFile.is_open()) {
                rFile.seekg(0, std::ios::end);
                if (rFile.tellg() > 0) fileExistsAndNotEmpty = true;
                rFile.close();
            }
        }

        std::ofstream csvFile(csvFilename, std::ios::app);
        if (csvFile.is_open()) {
            if (!fileExistsAndNotEmpty) {
                csvFile << "TraceFile,CacheLevel,Size,Hits,Misses,HitRate,ReadMiss,WriteMiss,ReadHit,WriteHit,RAM_Access,AMAT,CPI,P95,Jitter,Drops,Bandwidth" << std::endl;
            }

            auto writeLine = [&](const CacheLevel& c) {
                long long total = c.hits + c.misses;
                double rate = (total > 0) ? ((double)c.hits / total) * 100.0 : 0.0;
                csvFile << g_Config.CURRENT_TRACE_FILE << "," << c.name << "," << c.size << "," << c.hits << "," << c.misses << "," 
                        << std::fixed << std::setprecision(2) << rate << "," 
                        << c.readMisses << "," << c.writeMisses << "," 
                        << c.readHits << "," << c.writeHits << ","
                        << ramAccesses << "," << s.amat << "," 
                        << s.cpi << "," << s.p95_latency << "," << s.jitter << "," 
                        << s.drops << "," << s.bandwidth << std::endl;
            };

            writeLine(L1);
            writeLine(L2);
            writeLine(L3);
            csvFile.close();
        } 
    }
};

#endif // CACHE_SIM_H
