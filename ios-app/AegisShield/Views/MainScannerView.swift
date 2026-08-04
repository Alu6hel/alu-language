import SwiftUI

struct MainScannerView: View {
    @EnvironmentObject var viewModel: ScannerViewModel
    @State private var pulse: CGFloat = 1.0
    
    var body: some View {
        NavigationView {
            ZStack {
                // Background
                Color.black.edgesIgnoringSafeArea(.all)
                
                VStack(spacing: 40) {
                    Text("Aegis Mobile Shield")
                        .font(.largeTitle)
                        .fontWeight(.bold)
                        .foregroundColor(.white)
                    
                    Spacer()
                    
                    // Radar / Status Indicator
                    ZStack {
                        Circle()
                            .fill(statusColor.opacity(0.2))
                            .frame(width: 200, height: 200)
                            .scaleEffect(viewModel.currentState == .scanning ? pulse : 1.0)
                            .animation(viewModel.currentState == .scanning ? Animation.easeInOut(duration: 1.0).repeatForever(autoreverses: true) : .default, value: pulse)
                        
                        Circle()
                            .stroke(statusColor, lineWidth: 4)
                            .frame(width: 120, height: 120)
                        
                        Image(systemName: statusIcon)
                            .resizable()
                            .scaledToFit()
                            .frame(width: 50, height: 50)
                            .foregroundColor(statusColor)
                    }
                    .onAppear {
                        pulse = 1.3
                    }
                    
                    // Status Text
                    Text(statusText)
                        .font(.headline)
                        .foregroundColor(statusColor)
                    
                    Spacer()
                    
                    // Controls
                    VStack(spacing: 20) {
                        Button(action: {
                            if viewModel.currentState != .scanning {
                                viewModel.triggerManualScan()
                            }
                        }) {
                            Text(viewModel.currentState == .scanning ? "Scanning..." : "Deep Scan Device")
                                .font(.headline)
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(viewModel.currentState == .scanning ? Color.gray : Color.blue)
                                .cornerRadius(12)
                        }
                        .disabled(viewModel.currentState == .scanning)
                        
                        NavigationLink(destination: ThreatLogView()) {
                            Text("View Threat Logs")
                                .font(.headline)
                                .foregroundColor(.blue)
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.white.opacity(0.1))
                                .cornerRadius(12)
                        }
                    }
                    .padding(.horizontal, 30)
                    .padding(.bottom, 40)
                }
            }
        }
    }
    
    // UI Helpers based on state
    private var statusColor: Color {
        switch viewModel.currentState {
        case .idle: return .gray
        case .scanning: return .blue
        case .safe: return .green
        case .threat: return .red
        }
    }
    
    private var statusIcon: String {
        switch viewModel.currentState {
        case .idle: return "shield"
        case .scanning: return "magnifyingglass"
        case .safe: return "checkmark.shield.fill"
        case .threat: return "exclamationmark.shield.fill"
        }
    }
    
    private var statusText: String {
        switch viewModel.currentState {
        case .idle: return "System Idle"
        case .scanning: return "Analyzing Filesystem..."
        case .safe: return "System Secure"
        case .threat(let msg): return msg
        }
    }
}
