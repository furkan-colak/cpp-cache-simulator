#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include "CacheSim.h"

namespace fs = std::filesystem;

// ==========================================
// YARDIMCI FONKSIYONLAR (MAIN SADECE UI)
// ==========================================

// Klasordeki .trace dosyalarini bulur (Cross-platform C++17 std::filesystem)
std::vector<std::string> getTraceFiles(const std::string& exePath) {
    std::vector<std::string> files;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(exePath, ec)) {
        if (!ec && entry.is_regular_file() && entry.path().extension() == ".trace") {
            files.push_back(entry.path().filename().string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

// CSV Dosyalarini Siler (Cross-platform C++17 std::filesystem)
void clearResults() {
    std::cout << "\n[ISLEM] Gecmis sonuclar temizleniyor..." << std::endl;
    std::error_code ec1, ec2;
    bool del1 = fs::remove("simulation_results.csv", ec1);
    bool del2 = fs::remove("comparison_results.csv", ec2);
    
    if (del1) std::cout << " -> 'simulation_results.csv' silindi." << std::endl;
    else std::cout << " -> 'simulation_results.csv' bulunamadi veya silinemedi." << std::endl;
    
    if (del2) std::cout << " -> 'comparison_results.csv' silindi." << std::endl;
    else std::cout << " -> 'comparison_results.csv' bulunamadi veya silinemedi." << std::endl;
    
    std::cout << "[TAMAM] Temizlik islemi bitti.\nDevam etmek icin Enter'a basin...";
    std::cin.ignore(); std::cin.get();
}

struct RunResult {// Simülasyon sonucu
    MemorySystem::SimStats stats;
};

// std::pair yerine struct
RunResult runSimulation(const std::string& filename, bool silent = false) {
    if (filename.empty()) return {};

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[HATA] Trace dosyasi acilamadi: " << filename << std::endl;
        return {};
    }

    MemorySystem system; // MMap dosyalari constructor'da olusur
    g_Config.CURRENT_TRACE_FILE = filename; // for CSV reporting
    
    char type;
    std::uint64_t address;

    if (!silent) {
        std::cout << "Simulasyon baslatiliyor: " << filename << " (Algorithm: " << g_Config.ALGORITHM << ")" << std::endl;
        std::cout << "Lutfen bekleyin..." << std::endl;
    }

    // Hizli I/O
    std::ios_base::sync_with_stdio(false); //Bağlantıyı kapatır
    std::int64_t lineCount = 0;
    while (file >> type >> std::hex >> address) {
        if (type == 'I' || type == 'L' || type == 'R') {
            system.processTrace(address, LOAD);
        } else if (type == 'S' || type == 'W') {
            system.processTrace(address, STORE);
        } else if (type == 'M') {
            system.processTrace(address, LOAD);
            system.processTrace(address, STORE);
        }
        
        lineCount++;
        if (!silent && lineCount % 1000000 == 0) {
            std::cout << "\rIslenen satir: " << std::dec << lineCount << std::flush;
        }
    }
    
    if (!silent) {
        std::cout << "\rIsleme tamamlandi. Toplam satir: " << std::dec << lineCount << std::endl;
        system.report("simulation_results.csv");
    }
    
    file.close();
    
    auto stats = system.getStats();
    return { stats };
}

// Batch Scan
void scanDirectory(const std::string& exePath) {
    std::vector<std::string> files = getTraceFiles(exePath);
    if (files.empty()) {
        std::cout << "[HATA] Klasorde hic .trace dosyasi yok!" << std::endl;
        return;
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << "    BATCH MODU: TUM DOYALAR TARANIYOR     " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Algoritma : " << g_Config.ALGORITHM << std::endl;
    std::cout << "Prefetcher: " << (g_Config.PREFETCHER_ENABLED ? "ON" : "OFF") << std::endl;

    for (const auto& f : files) {
        std::cout << "\n>>> Isleniyor: " << f << " <<<" << std::endl;
        runSimulation(f); 
    }
    std::cout << "\n[OK] Tum dosyalar islendi." << std::endl;
}

// Performans Degisimini Analiz Eden Yardimci Fonksiyon
std::string evaluatePerformanceChange(double currentHitRate, double prevHitRate) {
    if (prevHitRate == -1.0) return "Referans";

    double gain = currentHitRate - prevHitRate;
    
    if (gain > 5.0) return "!!! KRITIK ARTIS";
    if (gain > 1.0) return "+ Verim Artisi";
    if (gain >= 0.1) return "+ Stabil Artis";
    if (gain > -0.1) return "Doyum Noktasi";
    
    return "! PERFORMANS KAYBI";
}

// Associativity Testi
void runAssociativityTest(const std::string& filename) {
    if (filename.empty()) return;
    
    std::cout << "\n";
    setColor(COLOR_CYAN);
    std::cout << " ========================================================= " << std::endl;
    std::cout << "         DONANIM ANALIZI: Associativity Etkisi             " << std::endl;
    std::cout << " ========================================================= " << std::endl;
    setColor(COLOR_DEFAULT);
    std::cout << " Test Edilen Dosya : " << filename << std::endl;
    std::cout << " Temel Ayarlar     : L1=" << (g_Config.L1_SIZE/1024) << "KB, Block=" << g_Config.BLOCK_SIZE << "B" << std::endl;
    
    std::vector<int> assocs = {1, 2, 4, 8, 16, 32};//64 yapacaksan long long kullan unutma
    int originalAssoc = g_Config.L1_ASSOC; // Yedekle

    std::cout << "\n";
    setColor(COLOR_YELLOW);
    std::cout << " +----------+--------------+--------------+-------------+------------------+" << std::endl;
    std::cout << " | Tasarim  | L1 Hit Rate  | Sim Cycles   | Bandwidth   | Analiz / Durum   |" << std::endl;
    std::cout << " +----------+--------------+--------------+-------------+------------------+" << std::endl;
    setColor(COLOR_DEFAULT);

    double prevHitRate = -1.0;

    for(int a : assocs) {
        g_Config.L1_ASSOC = a;
        RunResult res = runSimulation(filename, true);
        
        std::string assocName = std::to_string(a) + "-Way";
        if (a == 1) assocName = "Direct";
        
        std::cout << " | " << std::setw(8) << std::left << assocName 
                  << " | ";
        
        // Hit Rate
        if (res.stats.l1HitRate > 98.0) setColor(COLOR_GREEN);
        else if (res.stats.l1HitRate < 80.0) setColor(COLOR_RED);
        std::cout << std::setw(11) << std::fixed << std::setprecision(2) << res.stats.l1HitRate << "% ";
        setColor(COLOR_DEFAULT);

        std::cout << "| " << std::setw(12) << res.stats.cycles
                  << " | " << std::setw(8) << std::setprecision(2) << res.stats.bandwidth << " GB/s | ";

        // Analiz Sutunu
        std::string analysis = evaluatePerformanceChange(res.stats.l1HitRate, prevHitRate);
        
        if (analysis == "!!! KRITIK ARTIS") setColor(COLOR_GREEN);
        else if (analysis == "! PERFORMANS KAYBI") setColor(COLOR_RED);
        else if (analysis == "Doyum Noktasi") setColor(COLOR_YELLOW);
        
        std::cout << std::setw(16) << std::left << analysis;
        setColor(COLOR_DEFAULT);
        
        std::cout << " |" << std::endl;
        
        prevHitRate = res.stats.l1HitRate;
    }
    
    std::cout << " +----------+--------------+--------------+-------------+------------------+" << std::endl;
    
    g_Config.L1_ASSOC = originalAssoc; // Geri yukle
}

// Test Konfigurasyonu
struct TestConfig {
    ReplacementPolicy algorithm;
    bool prefetcher;
};

void compareAlgorithms(const std::string& filename, const std::vector<TestConfig>& configs) {
    if (configs.empty()) {
        std::cout << "[UYARI] Hicbir test konfigurasyonu secilmedi!" << std::endl;
        return;
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << "    ALGORITMA & PREFETCHER KARSILASTIRMA  " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << "Dosya: " << filename << std::endl;
    
    ReplacementPolicy originalAlgo = g_Config.ALGORITHM; // Yedekle ayarlar bozulmamasi icin
    bool originalPrefetch = g_Config.PREFETCHER_ENABLED;

    struct ComparisonEntry {
        std::string name; // Test Adı (e.g. "LRU+Prefetch")
        MemorySystem::SimStats stats;
    };
    std::vector<ComparisonEntry> results;

    // CSV Dosyasi Hazirligi
    std::string csvName = "comparison_results.csv";
    std::ofstream csvFile(csvName, std::ios::app);
    bool fileExists = (csvFile.tellp() != 0);
    if (!fileExists) {
        csvFile << "Algorithm,Prefetcher,TraceFile,TotalCycles,AMAT,CPI,L1_HitRate,P95_Latency,Jitter,Bandwidth" << std::endl;
    }

    for (const auto& cfg : configs) {
        g_Config.ALGORITHM = cfg.algorithm;
        g_Config.PREFETCHER_ENABLED = cfg.prefetcher;
        
        std::string displayName = std::string(getAlgorithmName(cfg.algorithm)) + (cfg.prefetcher ? " (P+)" : "");
        
        std::cout << "\n[" << displayName << "] Testi Calistiriliyor... "; 
        RunResult res = runSimulation(filename, true); // Silent
        std::cout << "TAMAMLANDI. (Cycles: " << res.stats.cycles << ")" << std::endl;
        results.push_back({displayName, res.stats});
        
        // CSV Yaz
        if (csvFile.is_open()) {
            csvFile << cfg.algorithm << "," << (cfg.prefetcher ? "ON" : "OFF") << "," << filename << "," 
                    << res.stats.cycles << "," 
                    << std::fixed << std::setprecision(4) << res.stats.amat << "," 
                    << res.stats.cpi << "," 
                    << res.stats.l1HitRate << "," 
                    << res.stats.p95_latency << "," 
                    << res.stats.jitter << "," 
                    << res.stats.bandwidth << std::endl;
        }
    }
    if (csvFile.is_open()) csvFile.close();
    std::cout << "[BILGI] Detayli veriler '" << csvName << "' dosyasina kaydedildi." << std::endl;

    g_Config.ALGORITHM = originalAlgo; // Geri yukle
    g_Config.PREFETCHER_ENABLED = originalPrefetch;

    // Analiz Tablosu (Gorsellestirilmis)
    std::cout << "\n";
    setColor(COLOR_YELLOW);
    std::cout << " +====================+==============+==========+==========+==========+======================+" << std::endl;
    std::cout << " | Model (P=Prefetch) | Toplam Cycle | AMAT     | Hit Rate | CPI      | Fark (vs ILK SECIM)  |" << std::endl;
    std::cout << " +====================+==============+==========+==========+==========+======================+" << std::endl;
    setColor(COLOR_DEFAULT);

    std::int64_t baseCycles = results[0].stats.cycles;

    for (const auto& r : results) {
         double diffPct = 0.0;
         if (baseCycles > 0) {
             double diff = (double)(r.stats.cycles - baseCycles);
             diffPct = (diff / baseCycles) * 100.0;
         }
         
         std::cout << " | " << std::setw(18) << std::left << r.name 
                   << " | " << std::setw(12) << r.stats.cycles
                   << " | " << std::setw(8) << std::fixed << std::setprecision(2) << r.stats.amat;
                   
         // Hit Rate Renklendirme
         if (r.stats.l1HitRate > 95.0) setColor(COLOR_GREEN);
         else if (r.stats.l1HitRate < 80.0) setColor(COLOR_RED);
         std::cout << " | " << std::setw(7) << r.stats.l1HitRate << "% ";
         setColor(COLOR_DEFAULT);
         
         std::cout << "| " << std::setw(8) << std::setprecision(2) << r.stats.cpi
                   << " | ";
         
         if (r.stats.cycles == baseCycles && r.name == results[0].name) {
              setColor(COLOR_CYAN);
              std::cout << "     REFERANS         ";
              setColor(COLOR_DEFAULT);
         }
         else if (diffPct > 0) {
              setColor(COLOR_RED);
              std::cout << "+" << std::setw(6) << diffPct << "% (Yavas)    ";
              setColor(COLOR_DEFAULT);
         }
         else if (diffPct < 0) {
              setColor(COLOR_GREEN);
              std::cout << std::setw(7) << diffPct << "% (Hizli)    ";
              setColor(COLOR_DEFAULT);
         }
         else {
             std::cout << "     AYNI             ";
         }
         
         std::cout << "|" << std::endl;
    }
    setColor(COLOR_YELLOW);
    std::cout << " +====================+==============+==========+==========+==========+======================+" << std::endl;
    setColor(COLOR_DEFAULT);
}

int main() {
    // Hizli I/O (Main basinda olmasi daha saglikli)
    std::ios_base::sync_with_stdio(false); 
    enableAnsiColors();
    
    // Random başlatıcı
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    
    std::string exePath = ".";
    if (!loadConfig("config.txt")) {
        std::cout << "Program sonlandiriliyor." << std::endl;
        return 1;
    }

    int choice;
    do {
        setColor(COLOR_CYAN);
        std::cout << "\n==========================================" << std::endl;
        std::cout << "    PERFORMANS CACHE SIMULATOR (V4.0 Modular)" << std::endl;
        std::cout << "==========================================" << std::endl;
        setColor(COLOR_DEFAULT);
        std::cout << "1. Tek Trace Dosyasi Calistir" << std::endl;
        std::cout << "2. Klasordeki Tum Dosyalari Tara (Batch)" << std::endl;
        std::cout << "3. Karsilastirma Modu (Detayli Analiz)" << std::endl;
        std::cout << "4. Donanim Analizi (Associativity Testi)" << std::endl; 
        std::cout << "5. Gecmis Sonuclari Temizle (CSV Sil)" << std::endl; 
        std::cout << "0. Cikis" << std::endl;
        std::cout << "Seciminiz: ";
        
        if (!(std::cin >> choice)) {
            if (std::cin.eof()) break; 
            std::cin.clear(); 
            std::string ignore; std::getline(std::cin, ignore);
            choice = -1;
        }

        std::string selectedFile;
        // Dosya Secimi (Option 1, 3 ve 4 icin ortak)
        if (choice == 1 || choice == 3 || choice == 4) {
            std::vector<std::string> files = getTraceFiles(exePath);
            if (files.empty()) { std::cout << "Trace dosyasi yok!" << std::endl; continue; }

            std::cout << "\nTrace Dosyasi Secin:" << std::endl;
            for (std::size_t i = 0; i < files.size(); i++) {
                std::cout << "[" << (i+1) << "] " << files[i] << std::endl;
            }
            std::cout << "Dosya seciniz (Numara): ";
            int fIdx;
            while(!(std::cin >> fIdx)) { 
                std::cin.clear(); 
                std::string x; 
                std::getline(std::cin, x); 
            }
            std::cin.ignore(1000, '\n'); // Tampon temizle (Enter karakterini yut)

            if (fIdx >= 1 && fIdx <= static_cast<int>(files.size())) selectedFile = files[fIdx-1];
            else { std::cout << "Gecersiz numara!" << std::endl; continue; }
            
            if (choice == 1) {
                // MODIFICATON FOR OPTION 1: Ask for Algorithm and Prefetcher
                std::cout << "\n--- Calistirma Ayarlari ---" << std::endl;
                setColor(COLOR_CYAN); std::cout << "Algoritma Secin:" << std::endl; setColor(COLOR_DEFAULT); 
                for (std::size_t i = 0; i < KNOWN_ALGORITHMS.size(); ++i) std::cout << (i + 1) << ". " << KNOWN_ALGORITHMS[i].name << std::endl;
                
                int algoChoice;
                std::cout << "Secim (1-" << KNOWN_ALGORITHMS.size() << "): "; 
                if (!(std::cin >> algoChoice)) { std::cin.clear(); std::string x; std::getline(std::cin, x); algoChoice = 1; }
                
                ReplacementPolicy selectedAlgo = g_Config.ALGORITHM;
                if (algoChoice >= 1 && algoChoice <= static_cast<int>(KNOWN_ALGORITHMS.size())) selectedAlgo = KNOWN_ALGORITHMS[algoChoice - 1].policy;
                
                int prefetchChoice; 
                std::cout << "Prefetcher Acik Olsun Mu? (1: Evet, 0: Hayir): "; 
                if (!(std::cin >> prefetchChoice)) { std::cin.clear(); std::string x; std::getline(std::cin, x); prefetchChoice = 0; }
                bool prefetchEnabled = (prefetchChoice == 1);
                
                // Gecici Config Guncelleme
                ReplacementPolicy oldAlgo = g_Config.ALGORITHM; 
                bool oldPrefetch = g_Config.PREFETCHER_ENABLED;
                
                g_Config.ALGORITHM = selectedAlgo; 
                g_Config.PREFETCHER_ENABLED = prefetchEnabled;
                
                runSimulation(selectedFile);
                
                // Config Geri Yukle
                g_Config.ALGORITHM = oldAlgo; 
                g_Config.PREFETCHER_ENABLED = oldPrefetch;
                
                std::cout << "\nDevam etmek icin Enter'a basin...";
                std::cin.ignore(1000, '\n'); // Olası kalan karakterleri temizle
                std::cin.get();
            }
            else if (choice == 4) {
                 runAssociativityTest(selectedFile); 
                 std::cout << "\nDevam etmek icin Enter'a basin...";
                 std::cin.get();
            }
        }
        
        if (choice == 3) { // Comparison mode logic
                std::vector<TestConfig> comparisonList;
                int subChoice = -1;
                while (subChoice != 9) {
                    setColor(COLOR_CYAN);
                    std::cout << "\n--- Karsilastirma Listesi Olustur ---" << std::endl;
                    setColor(COLOR_DEFAULT);
                    if (comparisonList.empty()) {
                        std::cout << "Mevcut Liste: [ BOS ]" << std::endl;
                    } else {
                        std::cout << "Mevcut Liste: [ ";
                        for(const auto& cfg : comparisonList) {
                             std::cout << cfg.algorithm;
                             if (cfg.prefetcher) std::cout << "(P+)";
                             std::cout << " ";
                        }
                        std::cout << "]" << std::endl;
                    }
                    for (std::size_t i = 0; i < KNOWN_ALGORITHMS.size(); ++i) {
                        std::cout << (i + 1) << ". " << KNOWN_ALGORITHMS[i].name << std::endl;
                    }
                    std::cout << "---------------------------" << std::endl;
                    std::cout << "8. SON EKLENENI SIL (Geri Al)" << std::endl;
                    std::cout << "9. KARSILASTIRMAYI BASLAT" << std::endl;
                    std::cout << "0. Iptal / Ana Menuye Don" << std::endl;
                    std::cout << "Secim (Ekle/Baslat): ";
                    
                    if (!(std::cin >> subChoice)) {
                        std::cin.clear(); std::string ignore; std::getline(std::cin, ignore); subChoice = -1;
                    }
                    if (subChoice >= 1 && subChoice <= static_cast<int>(KNOWN_ALGORITHMS.size())) {
                        const auto& algoInfo = KNOWN_ALGORITHMS[subChoice - 1];
                        std::cout << "   -> " << algoInfo.name << " icin Prefetcher Acik Olsun Mu? (1: Evet, 0: Hayir): ";
                        int pOp; 
                        if (!(std::cin >> pOp)) {std::cin.clear(); std::string x; std::getline(std::cin, x); pOp=0; }
                        bool pEnabled = (pOp == 1);
                        comparisonList.push_back({algoInfo.policy, pEnabled});
                    }
                    else if (subChoice == 8) {
                        if (!comparisonList.empty()) { comparisonList.pop_back(); } else { std::cout << "[UYARI] Liste zaten bos!" << std::endl; }
                    }
                    else if (subChoice == 9) {
                        if (comparisonList.empty()) { std::cout << "[UYARI] Liste bos! En az bir model secmelisiniz." << std::endl; subChoice = -1; } 
                    }
                    else if (subChoice == 0) { comparisonList.clear(); break; }
                }
                
                if(!comparisonList.empty()) {
                    compareAlgorithms(selectedFile, comparisonList);
                    std::cout << "\nDevam etmek icin Enter'a basin...";
                    std::cin.ignore(1000, '\n'); std::cin.get();
                }
        }
        else if (choice == 2) { // Tum dosyalari tara
            std::cout << "\n--- TUM TRACE DOSYALARINI TARA ---" << std::endl;
            setColor(COLOR_CYAN); std::cout << "Test icin Algoritma Secin:" << std::endl; setColor(COLOR_DEFAULT); 
            for (std::size_t i = 0; i < KNOWN_ALGORITHMS.size(); ++i) std::cout << (i + 1) << ". " << KNOWN_ALGORITHMS[i].name << std::endl;
            int algoChoice;
            std::cout << "Secim (1-" << KNOWN_ALGORITHMS.size() << "): "; 
            if(!(std::cin >> algoChoice)) { std::cin.clear(); std::string x; std::getline(std::cin, x); algoChoice = 1; }
            ReplacementPolicy selectedAlgoForScan = g_Config.ALGORITHM;
            if (algoChoice >= 1 && algoChoice <= static_cast<int>(KNOWN_ALGORITHMS.size())) selectedAlgoForScan = KNOWN_ALGORITHMS[algoChoice - 1].policy;
            
            int prefetchChoice; std::cout << "Prefetcher Acik Olsun Mu? (1: Evet, 0: Hayir): "; 
            if(!(std::cin >> prefetchChoice)) { std::cin.clear(); std::string x; std::getline(std::cin, x); prefetchChoice = 0; }
            bool prefetchEnabled = (prefetchChoice == 1);
            
            ReplacementPolicy oldAlgo = g_Config.ALGORITHM; bool oldPrefetch = g_Config.PREFETCHER_ENABLED;
            g_Config.ALGORITHM = selectedAlgoForScan; g_Config.PREFETCHER_ENABLED = prefetchEnabled;
            scanDirectory(exePath);
            g_Config.ALGORITHM = oldAlgo; g_Config.PREFETCHER_ENABLED = oldPrefetch;
            std::cout << "\nDevam etmek icin Enter'a basin..."; 
            std::cin.ignore(1000, '\n'); std::cin.get();
        } 
        else if (choice == 5) { clearResults(); }
        else if (choice == 0) { std::cout << "Cikis..." << std::endl; }
        else { std::cout << "Gecersiz secim." << std::endl; }
    } while (choice != 0);
    return 0;
}
