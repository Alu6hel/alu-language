import SwiftUI
import BackgroundTasks

@main
struct AegisShieldApp: App {
    @StateObject private var scannerVM = ScannerViewModel()
    
    init() {
        // Register the background task handler
        BGTaskScheduler.shared.register(forTaskWithIdentifier: "com.example.alu.AegisShield.deepScan", using: nil) { task in
            BackgroundScanner.shared.handleAppRefresh(task: task as! BGAppRefreshTask)
        }
    }

    var body: some Scene {
        WindowGroup {
            MainScannerView()
                .environmentObject(scannerVM)
                .onReceive(NotificationCenter.default.publisher(for: UIApplication.didEnterBackgroundNotification)) { _ in
                    BackgroundScanner.shared.scheduleAppRefresh()
                }
        }
    }
}
