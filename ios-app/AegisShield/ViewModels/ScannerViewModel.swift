import Foundation
import Combine

enum ScanState {
    case idle
    case scanning
    case safe
    case threat(String)
}

struct ThreatLog: Identifiable {
    let id = UUID()
    let filename: String
    let timestamp: Date
    let status: ScanState
}

class ScannerViewModel: ObservableObject {
    @Published var currentState: ScanState = .idle
    @Published var logs: [ThreatLog] = []
    
    // Simulate finding a file in the app's document directory or passed via Share Extension
    func triggerManualScan(filePath: String = "/var/mobile/Downloads/test_payload.apk") {
        currentState = .scanning
        
        // Push to background thread to avoid blocking UI
        DispatchQueue.global(qos: .userInitiated).async {
            // Call into Alu Native C/C++ backend via Swift Bridge
            let result = AluBridge.shared.scanFile(filePath: filePath)
            
            // Artificial delay for UI dramatic effect
            Thread.sleep(forTimeInterval: 1.5)
            
            DispatchQueue.main.async {
                if result {
                    self.currentState = .threat("YARA MATCH DETECTED")
                    self.logs.insert(ThreatLog(filename: filePath, timestamp: Date(), status: self.currentState), at: 0)
                } else {
                    self.currentState = .safe
                    self.logs.insert(ThreatLog(filename: filePath, timestamp: Date(), status: self.currentState), at: 0)
                }
            }
        }
    }
    
    func reset() {
        currentState = .idle
    }
}
